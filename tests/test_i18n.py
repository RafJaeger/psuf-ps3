"""Compile the translation paths on the host, without PS3 services or assets."""
import json
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
C_STRING = r'"(?:[^"\\]|\\.)*"'
TR_FUNCTION = r"^static const char \*tr\([^\n]*\)\n\{.*?^\}"
PATCH_TEXT_TRANSLATIONS = {
    "SUPPORT": "APOYAR",
    "CLOCK": "FRECUENCIA",
    "GPU core: %d MHz": "Núcleo GPU: %d MHz",
    "SCAN": "ESCANEAR",
    "ANALYSIS": "ANÁLISIS",
    "DATABASE": "BASE",
    "BACKUP": "RESPALDO",
    "SEARCH": "BUSCAR",
    "ONLINE": "EN LÍNEA",
    "Already runs at 60 FPS": "Ya funciona a 60 FPS",
    "No 60 FPS patch offered: the game already targets 60.": "No ofrece parche de 60 FPS: el juego ya tiene ese objetivo.",
    "Possible patch, unconfirmed": "Parche posible, sin confirmar",
    "Earlier test did not change FPS. Still UNSAFE.": "La prueba anterior no cambió los FPS. Sigue siendo NO SEGURO.",
    "Available": "Disponible",
    "Option saved during the last use.": "Opción guardada en el último uso.",
    "Applicable local pattern": "Patrón local aplicable",
    "Last scan found a local pattern for another version.": "El último escaneo halló un patrón local para otra versión.",
    "Saved 60 FPS pattern": "Patrón de 60 FPS guardado",
    "Saved 30 FPS pattern": "Patrón de 30 FPS guardado",
    "Saved FPS pattern": "Patrón de FPS guardado",
    "Last scan found an FPS hint, with no payload yet.": "El último escaneo halló una señal de FPS, aún sin payload.",
    "No EBOOT to analyze": "Sin EBOOT para analizar",
    "No game file found to test this version.": "No se halló un archivo del juego para probar esta versión.",
    "Applied without confirming the version. UNSAFE.": "Aplicado sin confirmar la versión. NO SEGURO.",
    "Another version patch applied without EBOOT analysis. UNSAFE.": "Parche de otra versión aplicado sin analizar EBOOT. NO SEGURO.",
    "Patch sent for HEN-safe testing. Temporary files are cleaned later.": "Parche enviado a prueba HEN-safe. Los temporales se borran después.",
    "PSUF forced pattern": "Patrón forzado por PSUF",
    "Forced pattern: 60 FPS": "Patrón forzado: 60 FPS",
    "Forced pattern: unlimited / +60 FPS": "Patrón forzado: FPS ilimitado / +60",
    "Probable payload generated from the local EBOOT on PS3. UNSAFE.": "Payload probable generado con el EBOOT local en PS3. NO SEGURO.",
    "Another version patch forced by the user. UNSAFE.": "Parche de otra versión forzado por el usuario. NO SEGURO.",
}


class TranslationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("cc") or shutil.which("gcc")
        if not compiler:
            raise unittest.SkipTest("Host C compiler required")
        cls.work = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.work.cleanup)
        build = Path(cls.work.name)
        (build / "sysutil").mkdir()
        (build / "sysutil/sysutil.h").write_text(
            "#include <stdint.h>\ntypedef int32_t s32;\n"
            "#define SYSUTIL_SYSTEMPARAM_ID_LANG 0\n"
            "int sysUtilGetSystemParamInt(int id, s32 *value);\n",
            encoding="utf-8",
        )
        main = (ROOT / "source/main.c").read_text(encoding="utf-8")
        function = re.search(TR_FUNCTION, main, re.M | re.S)
        if function is None:
            raise AssertionError("Cannot find main.c translation function")
        cls.pairs = list(dict.fromkeys(
            (json.loads(pt), json.loads(en))
            for pt, en in re.findall(
                r"\btr\(\s*[^,\n]+,\s*(" + C_STRING + r"),\s*(" + C_STRING + r")\s*\)",
                main[function.end():],
            )
        ))
        cls.dynamic = list(dict.fromkeys(
            json.loads(en) for en in re.findall(
                r'\btr\(\s*[^,\n]+,\s*[A-Za-z_][\w>.\-]*\s*,\s*(' + C_STRING + r")\s*\)",
                main[function.end():],
            )
        ))
        cls.cases = cls.pairs + [("Mensagem dinâmica em português", en) for en in cls.dynamic]
        cls.cases.append(("Mensagem dinâmica em português", "Could not start game"))
        cls.cases.extend(("Texto salvo em português", en) for en in PATCH_TEXT_TRANSLATIONS)
        rows = ",\n".join(
            "{" + json.dumps(pt, ensure_ascii=False) + ", " + json.dumps(en, ensure_ascii=False) + "}"
            for pt, en in cls.cases
        )
        harness = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "i18n.h"
#include "home_view.h"
#include "ui.h"
#include <sysutil/sysutil.h>
static int system_lang, system_error, dark;
int sysUtilGetSystemParamInt(int id, s32 *value) { *value = system_lang; return system_error; }
static void emit(const char *s) { fputs(s, stdout); fputc(0, stdout); }
int ui_is_dark(void) { return dark; }
void ui_draw_panel(int x,int y,int w,int h,int r,unsigned int c,unsigned int e) {}
void ui_fill_rect(int x,int y,int w,int h,unsigned int c) {}
void ui_draw_round_rect(int x,int y,int w,int h,int r,unsigned int c) {}
void ui_draw_disc(int x,int y,int r,unsigned int c) {}
void ui_draw_brand_logo(int x,int y,int w,int h) {}
void ui_draw_home_logo(int x,int y,int w,int h) {}
void ui_draw_icon(int x,int y,ui_icon icon,unsigned int c,unsigned int b) {}
void ui_draw_footer_actions(int x,int y,int width,const ui_footer_action *actions,int count) {}
void ui_draw_text(int x,int y,const char *s,unsigned int c,int scale)
{ printf("T:%d:%d:%d|",x,y,scale); emit(s); }
void ui_draw_text_fit(int x,int y,const char *s,unsigned int c,int scale,int width)
{ printf("F:%d:%d:%d:%d|",x,y,scale,width); emit(s); }
void ui_draw_text_box(int x,int y,const char *s,unsigned int c,int scale,int width,int height,int lines)
{ printf("B:%d:%d:%d:%d:%d:%d|",x,y,scale,width,height,lines); emit(s); }
__TR_FUNCTION__
static const char *pairs[][2] = { __PAIRS__ };
int main(int argc, char **argv)
{
    size_t i;
    int lang, id;
    if (argc < 2) return 1;
    if (!strcmp(argv[1], "pairs")) {
        for (i = 0; i < sizeof(pairs)/sizeof(pairs[0]); ++i)
            for (lang = 0; lang < 3; ++lang) emit(tr((fpsu_lang)lang, pairs[i][0], pairs[i][1]));
    } else if (!strcmp(argv[1], "i18n")) {
        for (lang = 0; lang < 3; ++lang)
            for (id = 0; id < TXT_MAX; ++id) emit(i18n_text((fpsu_lang)lang, (fpsu_text_id)id));
    } else if (!strcmp(argv[1], "detect")) {
        system_lang = atoi(argv[2]); system_error = atoi(argv[3]);
        printf("%d", i18n_detect_language());
    } else if (!strcmp(argv[1], "home")) {
        dark = atoi(argv[4]);
        home_view_draw((fpsu_lang)atoi(argv[2]), atoi(argv[3]), 1);
        printf("D|%d%c", dark, 0);
    } else if (!strcmp(argv[1], "fallback")) {
        emit(tr(FPSU_LANG_PT, "Português", "English"));
        emit(tr(FPSU_LANG_ES, "Português", "Unknown English key"));
        emit(tr((fpsu_lang)99, "Português", "English"));
        emit(i18n_text(FPSU_LANG_ES, (fpsu_text_id)-1));
        emit(i18n_text(FPSU_LANG_ES, TXT_MAX));
        emit(i18n_text((fpsu_lang)99, TXT_MENU_EXIT));
    } else return 2;
    return 0;
}
'''.replace("__TR_FUNCTION__", function.group()).replace("__PAIRS__", rows)
        (build / "harness.c").write_text(harness, encoding="utf-8")
        cls.binary = build / "translations.exe"
        result = subprocess.run([
            compiler, "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
            "-finput-charset=UTF-8", "-fexec-charset=UTF-8",
            "-I", str(build), "-I", str(ROOT / "include"), str(build / "harness.c"),
            str(ROOT / "source/home_view.c"), str(ROOT / "source/i18n.c"), "-o", str(cls.binary),
        ], capture_output=True, text=True, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stderr)

    def run_harness(self, *args):
        result = subprocess.run([str(self.binary), *map(str, args)], check=True,
                                capture_output=True, text=True, encoding="utf-8", timeout=10)
        return result.stdout

    def test_every_main_pair_and_dynamic_fallback_has_spanish(self):
        neutral = {"30 FPS", "Pix", "PNGs: pix_qr.png / paypal_qr.png", "Overclock",
                   "Downclock", "Title ID", "webMAN > Overclock"}
        records = self.run_harness("pairs").split("\0")[:-1]
        self.assertEqual(len(records), len(self.cases) * 3)
        for index, (pt, en) in enumerate(self.cases):
            with self.subTest(english=en):
                actual_en, actual_pt, actual_es = records[index * 3:index * 3 + 3]
                self.assertEqual(actual_en, en)
                self.assertEqual(actual_pt, pt)
                self.assertTrue(actual_es)
                if en not in neutral:
                    self.assertNotEqual(actual_es, en, "Spanish fell back to English")
                self.assertEqual(actual_es.count("\n"), en.count("\n"))

    def test_new_actions_theme_and_covers_have_short_spanish(self):
        records = self.run_harness("pairs").split("\0")[:-1]
        translated = {en: records[i * 3 + 2] for i, (_, en) in enumerate(self.cases)}
        expected = {"Start game": "Iniciar juego", "COVER": "CARÁTULA",
                    "Could not save the theme": "No se pudo guardar el tema",
                    "Could not write your preference on PS3. Try again.": "No se pudo guardar tu preferencia en PS3. Inténtalo de nuevo.",
                    "Delete": "Borrar", "Clear": "Limpiar", "Space": "Espacio",
                    "Canceling...": "Cancelando...",
                    "The installed games section could not be verified.": "No se pudo verificar la sección de juegos instalados."}
        for en, es in expected.items():
            self.assertEqual(translated[en], es)

    def test_reported_patch_texts_and_headings_translate_before_display_wiring(self):
        records = self.run_harness("pairs").split("\0")[:-1]
        translated = {en: records[i * 3 + 2] for i, (_, en) in enumerate(self.cases)}
        self.assertEqual(len(PATCH_TEXT_TRANSLATIONS), 31)
        for en, es in PATCH_TEXT_TRANSLATIONS.items():
            with self.subTest(english=en):
                self.assertEqual(translated[en], es)
                self.assertEqual(re.findall(r"%[a-zA-Z]", en), re.findall(r"%[a-zA-Z]", es))

    def test_i18n_tables_are_complete_and_use_v3(self):
        records = self.run_harness("i18n").split("\0")[:-1]
        ids = re.findall(r"^\s*(TXT_\w+)", (ROOT / "include/i18n.h").read_text(), re.M)
        count = len(ids) - 1  # TXT_MAX is the sentinel, not a message.
        self.assertEqual(len(records), 3 * count)
        self.assertTrue(all(records))
        for lang in range(3):
            self.assertIn("PSUF V3", records[(lang + 1) * count - 1])
            self.assertNotRegex(records[(lang + 1) * count - 1], r"PSUF V[12]")
        for en, es in zip(records[:count], records[2 * count:]):
            if en != "PSUF":
                self.assertNotEqual(en, es)

    def test_system_language_detection(self):
        for system_lang, expected in [(1, 0), (7, 1), (17, 1), (3, 2), (99, 0)]:
            self.assertEqual(self.run_harness("detect", system_lang, 0), str(expected))
        self.assertEqual(self.run_harness("detect", 3, -1), "0")

    def test_fallbacks_and_invalid_ids_are_unchanged(self):
        self.assertEqual(self.run_harness("fallback").split("\0")[:-1],
                         ["Português", "Unknown English key", "English", "", "", "Exit"])

    def test_home_descriptions_and_chrome_use_the_selected_language(self):
        for selected in range(11):
            descriptions = []
            for lang in range(3):
                records = self.run_harness("home", lang, selected, 0).split("\0")[:-1]
                description = [r.split("|", 1)[1] for r in records if r.startswith("B:")]
                self.assertEqual(len(description), 1)
                descriptions.append(description[0])
                headings = [r.split("|", 1)[1] for r in records if r.startswith("T:432:142:")]
                expected = (("LIBRARY", "BIBLIOTECA", "BIBLIOTECA") if selected < 3 else
                            ("BACKUPS", "BACKUPS", "RESPALDOS") if selected == 3 else
                            ("CONSOLE", "CONSOLE", "CONSOLA") if selected < 6 else
                            ("PSUF", "PSUF", "PSUF"))
                self.assertEqual(headings, [expected[lang]])
            self.assertEqual(len(set(descriptions)), 3, "Home description fell back to another language")
        for lang, color_words in [(0, ("teal/cyan", "black")), (1, ("azul-petróleo/ciano", "preto")),
                                  (2, ("azul petróleo/cian", "negro"))]:
            for dark in (0, 1):
                output = self.run_harness("home", lang, 9, dark)
                for word in color_words:
                    self.assertIn(word, output)
                self.assertIn(f"D|{dark}\0", output)

    def test_home_text_fits_the_bundled_font(self):
        font_path = ROOT / "release/USRDIR/font.uif"
        if not font_path.exists():
            self.skipTest("Bundled UI font required for width checks")
        data = font_path.read_bytes()
        self.assertEqual(data[:4], b"UIF1")
        offset, fonts = 8, []
        for scale in range(4):
            advances = {}
            for code in range(32, 256):
                width, height, advance, _ = data[offset:offset + 4]
                advances[chr(code)] = advance
                offset += 4 + width * height
            fonts.append(advances)

        def width(text, scale):
            return sum(fonts[scale - 1].get(c, fonts[scale - 1]["?"]) for c in text)

        for lang in range(3):
            for selected in range(11):
                records = self.run_harness("home", lang, selected, 0).split("\0")[:-1]
                for record in records:
                    position, _, text = record.partition("|")
                    with self.subTest(language=lang, text=text):
                        if position.startswith("F:99:"):
                            self.assertLessEqual(width(text, 2), 244, "Menu label needs a shorter translation")
                        if position.startswith("B:"):
                            lines = [""]
                            for word in text.split():
                                candidate = (lines[-1] + " " + word).strip()
                                if width(candidate, 2) > 735:
                                    lines.append(word)
                                else:
                                    lines[-1] = candidate
                            self.assertLessEqual(len(lines), 2, "Description would be truncated")


if __name__ == "__main__":
    unittest.main()
