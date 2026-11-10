import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class UiRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("cc") or shutil.which("gcc")
        if not compiler:
            raise unittest.SkipTest("Host C compiler required")
        cls.work = tempfile.TemporaryDirectory()
        cls.folder = Path(cls.work.name)
        cls.data = cls.folder / "settings"
        cls.binary = cls.folder / "ui-runtime"
        subprocess.run([
            compiler, "-std=gnu99", "-O1", "-DFPSU_UI_HOST", "-DFPSU_UI_TEST_PERSIST",
            f'-DFPSU_DATA_ROOT="{cls.data.as_posix()}"',
            f'-DFPSU_APP_USRDIR="{(ROOT / "release/USRDIR").as_posix()}"',
            "-include", str(ROOT / "tests/host/ui_types.h"),
            "-I", str(ROOT / "include"), str(ROOT / "tests/host/ui_runtime_harness.c"),
            "-lm", "-o", str(cls.binary),
        ], check=True, capture_output=True, text=True)
        source = (ROOT / "source/main.c").read_text(encoding="utf-8")
        selection = source.split("static const fpsu_patch_option *result_info_option(", 1)[1]
        selection = "static const fpsu_patch_option *result_info_option(" + selection.split(
            "\nstatic void draw_results(", 1)[0]
        harness = cls.folder / "result.c"
        harness.write_text('#include "app.h"\n' + selection + """
int main(void)
{
    fpsu_game_result result = {0};
    result.unlock.status = FPSU_STATUS_NOT_FOUND;
    result.fps60.status = FPSU_STATUS_KNOWN;
    result.fps30.status = FPSU_STATUS_OTHER_VERSION;
    if (result_info_option(&result) != &result.fps60) return 1;
    result.fps60.status = FPSU_STATUS_NOT_FOUND;
    if (result_info_option(&result) != &result.fps30) return 2;
    result.unlock.status = FPSU_STATUS_KNOWN;
    if (result_info_option(&result) != &result.unlock) return 3;
    return 0;
}
""", encoding="utf-8")
        cls.result_binary = cls.folder / "result"
        subprocess.run([compiler, "-std=gnu99", "-I", str(ROOT / "include"),
                        str(harness), "-o", str(cls.result_binary)],
                       check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.work.cleanup()

    def setUp(self):
        if self.data.exists():
            shutil.rmtree(self.data)

    def run_harness(self, *args):
        return subprocess.check_output([str(self.binary), *map(str, args)], text=True).strip()

    def test_theme_survives_new_process_and_repeated_changes(self):
        self.assertEqual(self.run_harness("theme"), "0 0")
        for dark in (1, 0, 1, 0):
            self.assertEqual(self.run_harness("theme", dark), f"0 {dark}")
            self.assertEqual(self.run_harness("theme"), f"0 {dark}")
            self.assertEqual((self.data / "theme.txt").read_text(), str(dark))

    def test_failed_theme_save_preserves_previous_preference(self):
        self.assertEqual(self.run_harness("theme", 0), "0 0")
        (self.data / "theme.tmp").mkdir()
        self.assertEqual(self.run_harness("theme", 1), "-1 0")
        self.assertEqual(self.run_harness("theme"), "0 0")

    def test_interrupted_theme_replacement_recovers_previous_preference(self):
        self.assertEqual(self.run_harness("theme", 1), "0 1")
        (self.data / "theme.txt").rename(self.data / "theme.previous")
        self.assertEqual(self.run_harness("theme"), "0 1")

    def test_original_and_black_palettes_have_light_text(self):
        original = [int(value, 16) for value in self.run_harness("colors", 0).split()]
        dark = [int(value, 16) for value in self.run_harness("colors", 1).split()]
        self.assertEqual(original[0], 0xFF0A1724)
        self.assertEqual(dark[0], 0xFF000000)
        self.assertGreater(original[2] & 255, 220)
        self.assertGreater(dark[2] & 255, 220)

    def test_cover_frame_matches_landscape_and_portrait_images_in_sd_and_hd(self):
        for screen_w, screen_h in ((720, 480), (1280, 720), (1920, 1080)):
            margin_x = screen_w // (20 if screen_w <= 720 else 40) if screen_w <= 1280 else 0
            margin_y = screen_h // (20 if screen_h <= 576 else 40) if screen_h <= 720 else 0
            view_w, view_h = screen_w - margin_x * 2, screen_h - margin_y * 2
            for source_w, source_h in ((320, 176), (300, 420), (256, 256)):
                width, height = map(int, self.run_harness(
                    "cover", screen_w, screen_h, source_w, source_h).split())
                self.assertLessEqual(width, 156)
                self.assertLessEqual(height, 226)
                aspect = (width * view_w / 1280) / (height * view_h / 720)
                self.assertAlmostEqual(aspect, source_w / source_h, delta=0.04)

    def test_game_summary_uses_available_fps_patch_before_missing_alternatives(self):
        subprocess.run([str(self.result_binary)], check=True)

    def test_header_and_home_animations_advance_independently(self):
        self.assertEqual(self.run_harness("animation"), "ok")
