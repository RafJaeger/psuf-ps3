import csv
import json
import os
import re
import shutil
import struct
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
USRDIR = ROOT / "release" / "USRDIR"

TITLE_ID_RE = re.compile(r"^[A-Z]{4}[0-9]{5}$")
VERSION_RE = re.compile(r"^(\*|[0-9]{2}\.[0-9]{2})$")
WRITE_RE = re.compile(r"^0\s+(?:0x)?[0-9A-Fa-f]{1,16}\s+[0-9A-Fa-f]{1,512}$")

PATCH_FIELDS = 10
GRAPHICS_FIELDS = 10
NATIVE_FIELDS = 2
VALID_KINDS = {"30", "60", "unlock"}
VALID_STATUSES = {
    "known",
    "untested",
    "pc",
    "other_version",
    "applicable",
    "unavailable",
    "native_60",
    "update_required",
}
VALID_METHODS = {"none", "ncl", "config", "eboot_pc", "pattern", "ncl_constant"}
NCL_METHODS = {"ncl", "ncl_constant"}
PUBLIC_FILES = [
    ROOT / "README.md",
    ROOT / "docs" / "ARCHITECTURE.md",
    ROOT / "docs" / "TECHNICAL_NOTES.md",
    USRDIR / "patches.csv",
    USRDIR / "graphics_patches.csv",
    USRDIR / "fix_patches.csv",
    USRDIR / "pre_patches.csv",
]
PATCH_DB_SOURCE = ROOT / "source" / "patch_db.c"


def read_rows(name):
    with (USRDIR / name).open(encoding="utf-8-sig", newline="") as handle:
        for line_no, row in enumerate(csv.reader(handle, delimiter="|"), 1):
            if not row or row[0].startswith("#"):
                continue
            yield line_no, [value.strip() for value in row]


def split_payload(payload):
    for raw in payload.replace("\n", ";").split(";"):
        item = raw.strip()
        if item:
            yield item


class PatchDatabaseTests(unittest.TestCase):
    def test_offline_pkg_database_precedes_older_saved_database(self):
        compiler = shutil.which("cc") or shutil.which("gcc")
        if not compiler:
            self.skipTest("Host C compiler required")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            bundled = work / "bundled"
            updated = work / "updated"
            bundled.mkdir()
            updated.mkdir()
            sample = "BCES00129|01.03|unlock|known|ncl|Jaeger|Jaeger|0 30473370 00000001|test|120\n"
            for root in (bundled, updated):
                (root / "patches.csv").write_text(sample, encoding="utf-8")
                (root / "native60.csv").write_text("BCES00129|test\n", encoding="utf-8")
            harness = work / "precedence.c"
            harness.write_text(
                '#include <stdio.h>\n#include <sys/stat.h>\n#include <dirent.h>\n'
                '#ifdef _WIN32\n#include <direct.h>\n#include <io.h>\n'
                '#define mkdir(path, mode) _mkdir(path)\n#define fsync _commit\n#endif\n'
                '#include "patch_db.c"\n'
                'int backup_path_is_safe_target(const char *path) { (void)path; return 0; }\n'
                'int backup_copy_file(const char *id, const char *source, char *out, size_t size) '
                '{ (void)id; (void)source; (void)out; (void)size; return -1; }\n'
                'fpsu_lang i18n_detect_language(void) { return 0; }\n'
                'const char *i18n_text(fpsu_lang lang, fpsu_text_id id) '
                '{ (void)lang; (void)id; return ""; }\n'
                'int main(void) { puts(patch_db_active_path()); '
                'puts(native60_db_active_path()); return 0; }\n',
                encoding="utf-8",
            )
            binary = work / "precedence"
            subprocess.run(
                [compiler, "-std=gnu99", "-ffunction-sections", "-Wl,--gc-sections",
                 "-I", str(ROOT / "include"), "-I", str(ROOT / "source"),
                 f'-DFPSU_APP_USRDIR="{bundled.as_posix()}"',
                 f'-DFPSU_DATA_ROOT="{updated.as_posix()}"',
                 str(harness), "-o", str(binary)],
                check=True,
            )

            def selected():
                return subprocess.check_output([str(binary)], text=True).splitlines()

            for root, stamp in ((bundled, 1700000200), (updated, 1700000100)):
                for name in ("patches.csv", "native60.csv"):
                    os.utime(root / name, (stamp, stamp))
            self.assertEqual(selected(), [(bundled / "patches.csv").as_posix(),
                                          (bundled / "native60.csv").as_posix()])
            for name in ("patches.csv", "native60.csv"):
                os.utime(updated / name, (1700000300, 1700000300))
            self.assertEqual(selected(), [(updated / "patches.csv").as_posix(),
                                          (updated / "native60.csv").as_posix()])
            (updated / "patches.csv").write_text("invalid\n", encoding="utf-8")
            self.assertEqual(selected()[0], (bundled / "patches.csv").as_posix())

    def test_other_region_of_manual_entry_shows_other_version(self):
        """NPUB31154 (GTA V, PSN USA) must point to the BLES01807 patches as another version."""
        compiler = shutil.which("cc") or shutil.which("gcc")
        if not compiler:
            self.skipTest("Host C compiler required")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            bundled = work / "bundled"
            updated = work / "updated"
            bundled.mkdir()
            updated.mkdir()
            for name in ("patches.csv", "graphics_patches.csv", "fix_patches.csv",
                         "pre_patches.csv", "native60.csv"):
                shutil.copy(USRDIR / name, bundled / name)
            harness = work / "cross.c"
            harness.write_text(
                '#include <stdio.h>\n#include <sys/stat.h>\n#include <dirent.h>\n'
                '#ifdef _WIN32\n#include <direct.h>\n#include <io.h>\n'
                '#define mkdir(path, mode) _mkdir(path)\n#define fsync _commit\n#endif\n'
                '#include "patch_db.c"\n'
                'int backup_path_is_safe_target(const char *path) { (void)path; return 0; }\n'
                'int backup_copy_file(const char *id, const char *source, char *out, size_t size) '
                '{ (void)id; (void)source; (void)out; (void)size; return -1; }\n'
                'fpsu_lang i18n_detect_language(void) { return 0; }\n'
                'const char *i18n_text(fpsu_lang lang, fpsu_text_id id) '
                '{ (void)lang; (void)id; return ""; }\n'
                'int main(int argc, char **argv) {\n'
                '    fpsu_game game; static fpsu_game_result result;\n'
                '    if (argc < 4) return 2;\n'
                '    memset(&game, 0, sizeof(game));\n'
                '    snprintf(game.title_id, sizeof(game.title_id), "%s", argv[1]);\n'
                '    snprintf(game.version, sizeof(game.version), "%s", argv[2]);\n'
                '    snprintf(game.title, sizeof(game.title), "%s", argv[3]);\n'
                '    patch_db_match_game(&game, &result);\n'
                '    printf("%d|%d|%s\\n", (int)result.fps60.status, (int)result.unlock.status,\n'
                '        result.fps60.note);\n'
                '    return 0;\n'
                '}\n',
                encoding="utf-8",
            )
            binary = work / "cross"
            subprocess.run(
                [compiler, "-std=gnu99", "-ffunction-sections", "-Wl,--gc-sections",
                 "-I", str(ROOT / "include"), "-I", str(ROOT / "source"),
                 f'-DFPSU_APP_USRDIR="{bundled.as_posix()}"',
                 f'-DFPSU_DATA_ROOT="{updated.as_posix()}"',
                 str(harness), "-o", str(binary)],
                check=True,
            )

            def match(title_id, version, title):
                out = subprocess.check_output([str(binary), title_id, version, title], text=True)
                fps60, unlock, note = out.split("|", 2)
                return int(fps60), int(unlock), note

            fps60, unlock, note = match("NPUB31154", "01.27", "Grand Theft Auto V")
            self.assertEqual(fps60, 4)  # FPSU_STATUS_OTHER_VERSION
            self.assertEqual(unlock, 4)
            self.assertIn("BLES01807", note)
            # The disc it comes from still gets the real patch.
            self.assertEqual(match("BLES01807", "01.27", "Grand Theft Auto V")[0], 1)
            # Only the exact name links titles: a longer name is a different game.
            self.assertEqual(match("NPEB09999", "01.00", "Grand Theft Auto V Premium Edition")[0], 0)

    def test_matching_a_game_reads_each_database_only_a_few_times(self):
        """Scanning must not reopen a database for every line (the PS3 scan looked frozen)."""
        compiler = shutil.which("cc") or shutil.which("gcc")
        if not compiler:
            self.skipTest("Host C compiler required")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            bundled = work / "bundled"
            updated = work / "updated"
            bundled.mkdir()
            updated.mkdir()
            names = ("patches.csv", "graphics_patches.csv", "fix_patches.csv",
                     "pre_patches.csv", "native60.csv")
            for name in names:
                shutil.copy(USRDIR / name, bundled / name)
                shutil.copy(USRDIR / name, updated / name)
                os.utime(bundled / name, (1700000100, 1700000100))
                os.utime(updated / name, (1700000200, 1700000200))  # online copy is newer
            harness = work / "opens.c"
            harness.write_text(
                '#include <stdio.h>\n#include <sys/stat.h>\n#include <dirent.h>\n'
                '#ifdef _WIN32\n#include <direct.h>\n#include <io.h>\n'
                '#define mkdir(path, mode) _mkdir(path)\n#define fsync _commit\n#endif\n'
                'static int g_opens;\n'
                'static FILE *counted_fopen(const char *path, const char *mode) '
                '{ ++g_opens; return fopen(path, mode); }\n'
                '#define fopen counted_fopen\n'
                '#include "patch_db.c"\n'
                '#undef fopen\n'
                'int backup_path_is_safe_target(const char *path) { (void)path; return 0; }\n'
                'int backup_copy_file(const char *id, const char *source, char *out, size_t size) '
                '{ (void)id; (void)source; (void)out; (void)size; return -1; }\n'
                'fpsu_lang i18n_detect_language(void) { return 0; }\n'
                'const char *i18n_text(fpsu_lang lang, fpsu_text_id id) '
                '{ (void)lang; (void)id; return ""; }\n'
                'int main(void) {\n'
                '    fpsu_game game; static fpsu_game_result result; int i;\n'
                '    for (i = 0; i < 3; ++i) {\n'
                '        memset(&game, 0, sizeof(game));\n'
                '        snprintf(game.title_id, sizeof(game.title_id), "%s", i ? "NPEB01283" : "BLES99999");\n'
                '        snprintf(game.version, sizeof(game.version), "01.27");\n'
                '        snprintf(game.title, sizeof(game.title), "%s", i ? "Grand Theft Auto V" : "Jogo Teste");\n'
                '        g_opens = 0;\n'
                '        patch_db_match_game(&game, &result);\n'
                '        printf("%d\\n", g_opens);\n'
                '    }\n'
                '    return 0;\n'
                '}\n',
                encoding="utf-8",
            )
            binary = work / "opens"
            subprocess.run(
                [compiler, "-std=gnu99", "-ffunction-sections", "-Wl,--gc-sections",
                 "-I", str(ROOT / "include"), "-I", str(ROOT / "source"),
                 f'-DFPSU_APP_USRDIR="{bundled.as_posix()}"',
                 f'-DFPSU_DATA_ROOT="{updated.as_posix()}"',
                 str(harness), "-o", str(binary)],
                check=True,
            )
            opens = [int(n) for n in subprocess.check_output([str(binary)], text=True).split()]
            # A few opens per database per game; the frozen scan reopened a file per line (hundreds).
            for count in opens:
                self.assertLess(count, 40, opens)

    def test_legacy_runtime_scripts_migrate_when_rename_cannot_replace(self):
        compiler = shutil.which("cc") or shutil.which("gcc")
        if not compiler:
            self.skipTest("Host C compiler required")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            harness = work / "migrate.c"
            harness.write_text(
                '#include <stdio.h>\n#include <sys/stat.h>\n#include <string.h>\n'
                '#ifdef _WIN32\n#include <dirent.h>\n#include <direct.h>\n#include <io.h>\n'
                '#define mkdir(path, mode) _mkdir(path)\n#define fsync _commit\n#endif\n'
                'static int gameos_rename(const char *, const char *);\n'
                '#define rename gameos_rename\n#include "patch_db.c"\n#undef rename\n'
                'int backup_path_is_safe_target(const char *path) { (void)path; return 0; }\n'
                'int backup_copy_file(const char *id, const char *source, char *out, size_t size) '
                '{ (void)id; (void)source; (void)out; (void)size; return -1; }\n'
                'static int fail_publish;\n'
                'static int gameos_rename(const char *src, const char *dst) {\n'
                '    struct stat st;\n'
                '    if (stat(dst, &st) == 0) { errno = EEXIST; return -1; }\n'
                '    if (fail_publish && strstr(src, ".psuf-tmp")) return -1;\n'
                '    return rename(src, dst);\n'
                '}\n'
                'fpsu_lang i18n_detect_language(void) { return 0; }\n'
                'const char *i18n_text(fpsu_lang lang, fpsu_text_id id) { return "PSUF"; }\n'
                'int main(int argc, char **argv) {\n'
                '    fail_publish = argc > 1;\n'
                '    guard_saved_ingame_scripts();\n'
                '    return 0;\n'
                '}\n',
                encoding="utf-8",
            )
            binary = work / "migrate"
            subprocess.run(
                [compiler, "-std=gnu99", "-ffunction-sections", "-Wl,--gc-sections",
                 "-I", str(ROOT / "include"), "-I", str(ROOT / "source"),
                 f'-DFPSU_WM_INGAME_ROOT="{work.as_posix()}"', str(harness), "-o", str(binary)],
                check=True,
            )
            script = work / "NPEA00280.bat"
            previous = work / "NPEA00280.bat.psuf-prev"
            original = ("# PS3 FPS Unlocker auto apply\n"
                        "logfile /dev_hdd0/FPSU/cache/webman_ingame.log\n"
                        "wait 9\nwrite 00161554 38800001\n")
            script.write_bytes(original.encode("ascii"))
            subprocess.run([str(binary), "fail"], check=True)
            self.assertEqual(script.read_text(), original)
            self.assertFalse(previous.exists())

            script.rename(previous)
            subprocess.run([str(binary)], check=True)
            migrated = script.read_text()
            self.assertIn("auto apply (compatible)", migrated)
            self.assertIn("if titleid NPEA00280", migrated)
            self.assertIn("write 00161554 38800001", migrated)
            self.assertNotIn("wait 9", migrated)
            self.assertFalse(previous.exists())

            guarded = work / "BLES01128.bat"
            guarded.write_bytes(
                b"# PS3 FPS Unlocker auto apply (guarded)\n"
                b"logfile /dev_hdd0/FPSU/cache/webman_ingame.log\n"
                b"abort if xmb\nabort if not titleid BLES01128\n"
                b"wait 2\nabort if xmb\nabort if not titleid BLES01128\n"
                b"write 00161554 38800001\n"
            )
            subprocess.run([str(binary)], check=True)
            upgraded = guarded.read_text()
            self.assertIn("auto apply (compatible)", upgraded)
            self.assertNotIn("abort if not titleid", upgraded)
            self.assertIn("write 00161554 38800001", upgraded)
            self.assertEqual(script.read_text(), migrated)

            def reaches_write(script_text, title, legacy):
                commands = script_text.splitlines()
                labels = {line[1:]: i for i, line in enumerate(commands) if line.startswith(":")}

                def condition(text):
                    if text == "xmb":
                        return title is None
                    if text == "exist /dev_hdd0":
                        return True
                    wanted = text.removeprefix("titleid ")
                    return legacy is not None and len(title or "") >= len(wanted) and (
                        (title[:len(wanted)] == wanted) != legacy
                    )

                i = 0
                while i < len(commands):
                    line = commands[i]
                    if line.startswith("write "):
                        return True
                    if line.startswith("abort if ") and condition(line[9:]):
                        return False
                    if line.startswith("if ") and not condition(line[3:]):
                        i = commands.index("end if", i + 1)
                    elif line.startswith("goto "):
                        i = labels[line[5:]]
                    i += 1
                return False

            for body, expected in [(migrated, "NPEA00280"), (upgraded, "BLES01128")]:
                for legacy in (False, True, None):
                    with self.subTest(title=expected, legacy=legacy):
                        self.assertEqual(reaches_write(body, expected, legacy), legacy is not None)
                        for other in ("", "NPUB31254", None):
                            self.assertFalse(reaches_write(body, other, legacy))

    def test_disc_patch_version_uses_app_ver_when_present(self):
        compiler = shutil.which("cc") or shutil.which("gcc")
        if not compiler:
            self.skipTest("Host C compiler required")
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            harness = work / "sfo_check.c"
            harness.write_text(
                '#include <stdio.h>\n#include "sfo.h"\n'
                'int main(int argc, char **argv) {\n'
                '    fpsu_game game;\n'
                '    if (argc != 2 || sfo_read_game(argv[1], &game) != 0) return 1;\n'
                '    puts(game.version);\n'
                '    return 0;\n}\n',
                encoding="utf-8",
            )
            binary = work / "sfo_check.exe"
            subprocess.run(
                [compiler, "-I", str(ROOT / "include"), str(harness),
                 str(ROOT / "source/sfo.c"), "-o", str(binary)],
                check=True,
            )
            for app_ver, expected in [(None, "01.00"), ("00.00", "01.00"), ("01.01", "01.01")]:
                with self.subTest(app_ver=app_ver):
                    fields = {"CATEGORY": "DG", "TITLE_ID": "NPEA00280", "VERSION": "01.00"}
                    if app_ver:
                        fields["APP_VER"] = app_ver
                    keys, values, entries = bytearray(), bytearray(), bytearray()
                    for name, value in fields.items():
                        encoded = value.encode("ascii") + b"\0"
                        entries += struct.pack("<HHIII", len(keys), 0x0204,
                                               len(encoded), len(encoded), len(values))
                        keys += name.encode("ascii") + b"\0"
                        values += encoded
                    key_table = 20 + len(entries)
                    sfo = work / "PARAM.SFO"
                    sfo.write_bytes(struct.pack("<4sIIII", b"\0PSF", 0x101, key_table,
                                                key_table + len(keys), len(fields))
                                    + entries + keys + values)
                    result = subprocess.check_output([str(binary), str(sfo)], text=True)
                    self.assertEqual(result.strip(), expected)

    def test_fps_patch_rows_are_valid(self):
        for line_no, row in read_rows("patches.csv"):
            self.assertIn(len(row), {PATCH_FIELDS - 1, PATCH_FIELDS}, line_no)
            row += [""] * (PATCH_FIELDS - len(row))
            title_id, version, kind, status, method, label, source, payload, _note, delay = row

            self.assertRegex(title_id, TITLE_ID_RE, line_no)
            self.assertRegex(version, VERSION_RE, line_no)
            self.assertIn(kind, VALID_KINDS, line_no)
            self.assertIn(status, VALID_STATUSES, line_no)
            self.assertIn(method, VALID_METHODS, line_no)
            self.assertTrue(label, line_no)
            self.assertTrue(source, line_no)
            if method in NCL_METHODS:
                self.assertTrue(payload, line_no)
            self.assert_payload_is_valid(payload, line_no)
            self.assert_delay_is_valid(delay, line_no)

    def test_graphics_patch_rows_are_valid(self):
        for line_no, row in read_rows("graphics_patches.csv"):
            self.assertEqual(len(row), GRAPHICS_FIELDS, line_no)
            title_id, version, game, label, status, method, source, payload, _note, delay = row

            self.assertRegex(title_id, TITLE_ID_RE, line_no)
            self.assertRegex(version, VERSION_RE, line_no)
            self.assertTrue(game, line_no)
            self.assertTrue(label, line_no)
            self.assertIn(status, VALID_STATUSES, line_no)
            self.assertIn(method, VALID_METHODS, line_no)
            self.assertTrue(source, line_no)
            if method in NCL_METHODS:
                self.assertTrue(payload, line_no)
            self.assert_payload_is_valid(payload, line_no)
            self.assert_delay_is_valid(delay, line_no)

    def test_companion_fix_rows_are_valid(self):
        for line_no, row in read_rows("fix_patches.csv"):
            self.assertEqual(len(row), PATCH_FIELDS, line_no)
            title_id, version, kind, status, method, label, source, payload, _note, delay = row

            self.assertRegex(title_id, TITLE_ID_RE, line_no)
            self.assertRegex(version, VERSION_RE, line_no)
            self.assertIn(kind, VALID_KINDS, line_no)
            self.assertIn(status, VALID_STATUSES, line_no)
            self.assertIn(method, VALID_METHODS, line_no)
            self.assertTrue(label, line_no)
            self.assertTrue(source, line_no)
            self.assert_payload_is_valid(payload, line_no)
            self.assert_delay_is_valid(delay, line_no)

    def test_native60_rows_are_valid(self):
        for line_no, row in read_rows("native60.csv"):
            self.assertGreaterEqual(len(row), 1, line_no)
            self.assertLessEqual(len(row), NATIVE_FIELDS, line_no)
            self.assertRegex(row[0], TITLE_ID_RE, line_no)

    def test_no_exact_duplicate_patch_routes(self):
        for file_name, key_size in (
            ("patches.csv", 8),
            ("graphics_patches.csv", 8),
            ("fix_patches.csv", 8),
        ):
            seen = {}
            for line_no, row in read_rows(file_name):
                key = tuple(row[:key_size])
                self.assertNotIn(key, seen, f"{file_name}:{line_no} duplicates line {seen.get(key)}")
                seen[key] = line_no

    def test_rpcs3_rows_are_hidden_when_same_version_is_tested(self):
        fps_rows = [(line_no, row) for line_no, row in read_rows("patches.csv")]
        tested_versions = {
            (row[0], row[1])
            for _line_no, row in fps_rows
            if len(row) >= 7 and row[3] == "known" and "rpcs3" not in row[6].lower()
        }

        for line_no, row in fps_rows:
            if len(row) >= 7 and "rpcs3" in row[6].lower():
                self.assertNotIn((row[0], row[1]), tested_versions, line_no)

    def test_database_package_contains_only_expected_files(self):
        import importlib.util

        spec = importlib.util.spec_from_file_location("build_psufdb", ROOT / "tools" / "build_psufdb.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)

        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "database.psufdb"
            old_argv = list(__import__("sys").argv)
            try:
                __import__("sys").argv = ["build_psufdb.py", "test", str(out)]
                module.main()
            finally:
                __import__("sys").argv = old_argv

            with zipfile.ZipFile(out) as archive:
                names = set(archive.namelist())
                self.assertEqual(names, {"manifest.json", "patches.csv", "graphics_patches.csv", "native60.csv", "fix_patches.csv", "pre_patches.csv"})
                manifest = json.loads(archive.read("manifest.json").decode("utf-8"))
                self.assertEqual(manifest["format"], "PSUF database update v1")

    def test_public_files_do_not_contain_private_local_data(self):
        blocked = ["client_secret", "bot token", "discord_bot_token"]
        for path in PUBLIC_FILES:
            text = path.read_text(encoding="utf-8", errors="ignore").lower()
            for token in blocked:
                self.assertNotIn(token.lower(), text, str(path))

    def test_patch_database_keeps_legacy_routes_except_replaced_gta_v(self):
        fps_rows = [row for _line_no, row in read_rows("patches.csv")]
        graphics_rows = [row for _line_no, row in read_rows("graphics_patches.csv")]
        fps_text = "\n".join("|".join(row) for row in fps_rows)

        self.assertIn("RPCS3 converted USERLIST", fps_text)
        self.assertNotIn("BLUS31162", fps_text)
        self.assertTrue(any(row[0] == "BLUS31162" and row[2] == "Battlefield 4" for row in graphics_rows))

        gta_v = [row for row in fps_rows if row[0] == "BLES01807"]
        self.assertEqual(len(gta_v), 2)
        self.assertIn([
            "01.27",
            "60",
            "known",
            "ncl",
            "60 FPS (testado por mnz)",
            "mnz / PSUF community",
            "0 004A0E34 4800006C;0 01F891B4 00000001",
        ], [row[1:8] for row in gta_v])
        self.assertIn([
            "01.27",
            "unlock",
            "known",
            "ncl",
            "FPS Unlock (Joey85)",
            "Joey85",
            "0 00BD3758 38800001",
        ], [row[1:8] for row in gta_v])
        self.assertFalse(any(row[0] in {"BLJM61019", "BLUS31156"} for row in fps_rows))

        self.assertTrue(any(row[0] == "BCUS98174" and row[1] == "01.11" and row[7] == "0 01571A6F 01" for row in fps_rows))
        self.assertTrue(any(row[0] == "BCUS98174" and row[1] == "01.11" and row[7] == "0 01571A6F 00" for row in fps_rows))

    def test_2529_new_tested_fps_patches_are_present(self):
        fps_rows = [row for _line_no, row in read_rows("patches.csv")]
        expected = {
            ("BLUS30612", "01.00", "60", "0 015CB94C 4800006C"),
            ("BLUS31518", "01.02", "60", "0 01C98430 00000001"),
            ("BLUS30538", "01.04", "unlock", "0 0001036C 38600000;0 01689708 00000000"),
            ("NPUB30830", "01.01", "60", "0 007462A4 28050001"),
            ("NPUB30389", "02.00", "60", "0 00730528 48000020"),
            ("BLES00669", "01.02", "60", "0 00E6F028 38000001"),
            ("BLES02096", "01.08", "60", "0 00C69318 4800012C;0 002466E4 C0243E68"),
            ("BLES02145", "*", "60", "0 0039D514 39200001"),
            ("NPUB30560", "*", "60", "0 00C40124 00000001"),
            ("NPUB31236", "*", "60", "0 015B5D9C 00000001"),
            ("BLUS31584", "*", "60", "0 006291C8 3860003C"),
            ("NPUB30984", "01.06", "60", "0 010E42C8 60000000;0 010E42DC 60000000"),
            ("NPUB30984", "*", "60", "0 010E42C8 60000000;0 010E42DC 60000000"),
            ("BLES00949", "01.02", "60", "0 3A3168E8 01"),
            ("BLJS10318", "01.05", "60", "0 0142E790 3BA90002;0 0236FEF0 3F000000;0 02370360 3F000000;0 023AED1C 3F000000"),
            ("BLJM60020", "*", "60", "0 00363D6C 3860003B;0 00150240 C1BE0784"),
            ("NPUB30471", "01.00", "60", "0 00D5B584 00000001;0 004C5220 60000000;0 004C5240 48000084;0 004C3F0C 386003E8"),
            ("BLES00909", "02.03", "unlock", "0 0059CC90 3BA00000"),
            ("NPEA00280", "01.01", "unlock", "0 00161554 38800001;0 0032CFB4 2B800003;0 300255A4 00000001;0 30025575 01"),
            ("BLES00107", "01.00", "unlock", "0 001E08D8 38000000"),
            ("BLES00237", "01.30", "unlock", "0 1009B361 00"),
            ("BLES00673", "01.01", "unlock", "0 017791CC 00000001"),
            ("BLES00016", "01.02", "unlock", "0 1011339C 00000001"),
            ("BLUS31207", "01.06", "unlock", "0 0178F990 00000000"),
            ("NPUB30589", "01.01", "unlock", "0 00E7128C 0000003C;0 006EB5D8 3C8000C7;0 006EB5E0 C044CD30;0 006EBD08 3C6000C7;0 006EBD18 C023CD30"),
            ("BLUS31065", "01.00", "unlock", "0 001E8058 60000000"),
        }
        found = {(row[0], row[1], row[2], row[7]) for row in fps_rows if len(row) >= 8}
        for item in expected:
            self.assertIn(item, found)

    def test_gtav_visual_profiles_are_graphics_only(self):
        fps_rows = [row for _line_no, row in read_rows("patches.csv")]
        graphics_rows = [row for _line_no, row in read_rows("graphics_patches.csv")]
        bad_terms = ("shadow", "draw", "distance", "lod", "smooth fps", "lite")

        for row in fps_rows:
            if row[0] == "BLES01807":
                text = "|".join(row).lower()
                self.assertFalse(any(term in text for term in bad_terms), row)

        gtav_graphics = [row for row in graphics_rows if row[0] == "BLES01807" and row[1] == "01.27"]
        self.assertGreaterEqual(len(gtav_graphics), 3)
        self.assertTrue(any("mnz" in row[6].lower() for row in gtav_graphics))

        bo2_graphics = [row for row in graphics_rows if row[0] == "BLUS31141" and row[1] == "01.19"]
        self.assertTrue(any(row[7] == "0 01708B40 C47A0000;0 01708C00 C47A0000;0 0170CE00 00" for row in bo2_graphics))

        fight_graphics = [row for row in graphics_rows if row[0] == "BLJM60020"]
        self.assertTrue(any(row[3] == "Quincunx" and row[7] == "0 008773A0 554A0020" for row in fight_graphics))
        self.assertTrue(any(row[3] == "Sweat" and row[7].startswith("0 10037120 00000000;") for row in fight_graphics))
        self.assertTrue(any(row[3] == "Spit / Impact particles" and row[7] == "0 100370F0 00000000;0 10037308 00000000;0 10037328 00000000;0 10037348 00000000" for row in fight_graphics))
        self.assertTrue(any(row[0] == "BLES00237" and row[1] == "01.30" and row[3] == "VSync Off" and row[6] == "Jaeger" and row[7] == "0 1009B360 00" for row in graphics_rows))
        self.assertTrue(any(row[0] == "BLES00016" and row[1] == "01.02" and row[3] == "VSync Off" and row[6] == "Jaeger" and row[7] == "0 005C72BC 38600001;0 005C72F0 38600001" for row in graphics_rows))

    def test_official_apply_keeps_persistent_runtime_script_and_cleans_start_trigger(self):
        text = PATCH_DB_SOURCE.read_text(encoding="utf-8", errors="ignore")
        self.assertNotIn("/artemis.ps3?attach", text)
        self.assertIn("/setmem.ps3mapi?addr=", text)
        self.assertIn('#define FPSU_WM_INGAME_ROOT "/dev_hdd0/tmp/wm_ingame"', text)
        self.assertIn('FPSU_WM_INGAME_ROOT "/%s.bat"', text)
        self.assertIn("runtime scripts are kept so webMAN can reapply the selected patch later", text)

    def assert_payload_is_valid(self, payload, line_no):
        for write in split_payload(payload):
            self.assertRegex(write, WRITE_RE, line_no)
            _zero, address, value = write.split(maxsplit=2)
            clean_address = address.upper().removeprefix("0X")
            clean_value = value.upper()
            self.assertLessEqual(len(clean_address), 16, line_no)
            self.assertEqual(len(clean_value) % 2, 0, line_no)

    def assert_delay_is_valid(self, delay, line_no):
        if not delay:
            return
        seconds = int(delay)
        self.assertGreaterEqual(seconds, 0, line_no)
        self.assertLessEqual(seconds, 600, line_no)


if __name__ == "__main__":
    unittest.main()
