import csv
import json
import re
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
]


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
                self.assertEqual(names, {"manifest.json", "patches.csv", "graphics_patches.csv", "native60.csv", "fix_patches.csv"})
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
        self.assertEqual(len(gta_v), 1)
        self.assertEqual(gta_v[0][1:8], [
            "01.27",
            "60",
            "known",
            "ncl",
            "60 FPS (testado por mnz)",
            "mnz / PSUF community",
            "0 004A0E34 4800006C;0 01F891B4 00000001",
        ])
        self.assertFalse(any(row[0] in {"BLJM61019", "BLUS31156"} for row in fps_rows))

        self.assertTrue(any(row[0] == "BCUS98174" and row[1] == "01.11" and row[7] == "0 01571A6F 01" for row in fps_rows))
        self.assertTrue(any(row[0] == "BCUS98174" and row[1] == "01.11" and row[7] == "0 01571A6F 00" for row in fps_rows))

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
