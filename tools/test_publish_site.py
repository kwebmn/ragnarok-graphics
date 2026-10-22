#!/usr/bin/env python3
"""Tests of tools/publish_site.py (2.10): the site's update.ini as core/update_check reads it (format 1)."""
import hashlib
import os
import sys
import tempfile
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import publish_site  # noqa: E402


def write(folder, name, data):
    path = Path(folder) / name
    path.write_bytes(data)
    return str(path)


class PublishSite(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.src = Path(self.tmp.name) / "src"
        self.web = Path(self.tmp.name) / "web"
        self.src.mkdir()
        self.zip = write(self.src, "uaro-graphics.zip", b"PK package 2.10.0")
        self.setup = write(self.src, "uaro-graphics-setup-full.exe", b"MZ setup")
        self.dlss = write(self.src, "uaro-graphics-dlss.zip", b"PK dlss")
        self.models = write(self.src, "uaro-graphics-models.zip", b"PK models")

    def tearDown(self):
        self.tmp.cleanup()

    def ini(self):
        return (self.web / "update.ini").read_bytes().decode("ascii")

    def test_the_manifest_names_the_archives_as_the_app_reads_them(self):
        publish_site.publish(str(self.web), "2.10.0", self.zip, self.setup, dlss=(self.dlss, "streamline 2.14.1"),
                             models=(self.models, "gameai-2.0 1"))
        text = self.ini()
        self.assertIn("\r\n[release]\r\nformat = 1\r\nversion = 2.10.0\r\n", text)
        self.assertIn("[package]\r\nfile = uaro-graphics-2.10.0.zip\r\nsize = 17\r\nsha256 = " +
                      hashlib.sha256(b"PK package 2.10.0").hexdigest() + "\r\n", text)
        self.assertIn("[dlss]\r\nversion = streamline 2.14.1\r\nfile = uaro-graphics-dlss-streamline-2.14.1.zip\r\n",
                      text)
        self.assertIn("[models]\r\nversion = gameai-2.0 1\r\nfile = uaro-graphics-models-gameai-2.0-1.zip\r\n", text)
        self.assertNotIn("withdrawn", text)
        for name in ("uaro-graphics-2.10.0.zip", "uaro-graphics-setup.exe", "uaro-graphics-dlss-streamline-2.14.1.zip",
                     "uaro-graphics-models-gameai-2.0-1.zip"):
            self.assertTrue((self.web / name).is_file(), name)

    def test_the_newest_five_packages_stay(self):
        for n in range(7):
            zip_path = write(self.src, "uaro-graphics.zip", b"PK package %d" % n)
            publish_site.publish(str(self.web), "2.10.%d" % n, zip_path, self.setup)
            os.utime(self.web / ("uaro-graphics-2.10.%d.zip" % n), (time.time() + n, time.time() + n))
        kept = sorted(p.name for p in self.web.glob("uaro-graphics-2.*.zip"))
        self.assertEqual(kept, ["uaro-graphics-2.10.%d.zip" % n for n in range(2, 7)])

    def test_a_withdrawn_release_sends_its_players_back(self):
        publish_site.publish(str(self.web), "2.10.1", self.zip, self.setup, dlss=(self.dlss, "streamline 2.14.1"))
        publish_site.publish(str(self.web), "2.10.0", self.zip, self.setup, withdraw=["2.10.1"])
        text = self.ini()
        self.assertIn("version = 2.10.0\r\nwithdrawn = 2.10.1\r\n", text)
        self.assertIn("[dlss]\r\nversion = streamline 2.14.1\r\n", text)  # kept from the release before

    def test_the_self_test_passes(self):
        self.assertEqual(publish_site.self_test(), 0)


if __name__ == "__main__":
    unittest.main(verbosity=1)
