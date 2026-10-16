import http.server
import shutil
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ROW = "NPUB30991|01.01|60|known|ncl|FPS Unlock|PSUF community|0 00B732D0 60000000;0 00B732E4 60000000|Test|120\n"


class OnlineUpdateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("cc") or shutil.which("gcc")
        if not compiler:
            raise unittest.SkipTest("Host C compiler required; run these tests in the build container")
        cls.build = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.build.name) / "update_harness"
        subprocess.run([
            compiler, "-std=gnu99", "-O1", "-ffunction-sections", "-fdata-sections",
            "-Wl,--gc-sections", "-I", str(ROOT / "tests/host"), "-I", str(ROOT / "include"),
            str(ROOT / "tests/host/update_harness.c"), "-o", str(cls.binary),
        ], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.addCleanup(self.work.cleanup)
        self.directory = Path(self.work.name)

    def run_harness(self, *args):
        return subprocess.run([str(self.binary), *map(str, args)], capture_output=True, text=True, timeout=25)

    def assert_selected(self, updated, expected_updated):
        bundled = self.directory / "bundled.csv"
        downloaded = self.directory / "updated.csv"
        bundled.write_text(ROW * 6)
        if updated is not None:
            downloaded.write_text(updated)
        result = self.run_harness("active", bundled, downloaded)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), str(downloaded if expected_updated else bundled))

    def test_smaller_valid_update_overrides_bundled_database(self):
        self.assert_selected(ROW, True)

    def test_invalid_row_after_four_valid_rows_rejects_update(self):
        self.assert_selected(ROW * 5 + "broken row\n", False)

    def test_missing_update_uses_bundled_database(self):
        self.assert_selected(None, False)

    def test_oversized_row_rejects_update(self):
        self.assert_selected(ROW + "x" * 9000 + "\n", False)

    def test_update_urls_do_not_reuse_the_same_cache_key(self):
        result = self.run_harness("urls")
        self.assertEqual(result.returncode, 0)
        first, second = result.stdout.splitlines()
        self.assertNotEqual(first, second)
        self.assertTrue(first.startswith("http://example.com/patches.csv?psuf="))

    def download(self, body, declared_size=None, status=200, encoding=None, target=None):
        requests = []

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                requests.append(dict(self.headers))
                self.send_response(status)
                if declared_size is not None:
                    self.send_header("Content-Length", str(declared_size))
                if encoding:
                    self.send_header("Transfer-Encoding", encoding)
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, *args):
                pass

        server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        target = target or self.directory / "download.csv"
        try:
            result = self.run_harness("download", f"http://127.0.0.1:{server.server_port}/patches.csv", target)
        finally:
            server.shutdown()
            thread.join()
            server.server_close()
        return result, target, requests

    def test_complete_download_preserves_all_bytes(self):
        body = (ROW * 70).encode()
        result, target, requests = self.download(body, len(body))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(target.read_bytes(), body)
        self.assertEqual(requests[0]["Cache-Control"], "no-cache")

    def test_http10_body_without_length_is_supported(self):
        body = ROW.encode()
        result, target, _ = self.download(body)
        self.assertEqual(result.returncode, 0)
        self.assertEqual(target.read_bytes(), body)

    def test_truncated_download_is_removed(self):
        body = ROW.encode()
        result, target, _ = self.download(body, len(body) + 10)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(target.exists())

    def test_http_error_is_not_saved_as_database(self):
        result, target, _ = self.download(b"Not found" * 20, status=404)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(target.exists())

    def test_unexpected_chunked_response_is_rejected(self):
        body = ROW.encode()
        result, target, _ = self.download(body, encoding="chunked")
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(target.exists())


if __name__ == "__main__":
    unittest.main()
