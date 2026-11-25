"""Exercise the real Bash bootstrap with isolated package/build stand-ins.

Run on Linux: python3 -m unittest discover -s tests -p test_windows_bootstrap.py
No Windows installer, download, compiler or audio device is started.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class BootstrapTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="Tux ' radio & test ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        packaging = self.root / "packaging"
        packaging.mkdir()
        for name in ("auto-build-msys2.sh", "install-windows-tools.sh"):
            shutil.copy2(ROOT / "packaging" / name, packaging / name)
        self.marker = self.root / "built"
        self.events = self.root / "events.jsonl"
        (packaging / "build-windows.sh").write_text(
            '#!/usr/bin/env bash\nset -euo pipefail\n'
            'printf "built" > "$TUX_TEST_BUILD_MARKER"\n'
        )
        binary = self.root / "bin"
        binary.mkdir()
        pacman = binary / "pacman"
        pacman.write_text(
            '#!/usr/bin/env python3\n'
            'import json,os,sys\n'
            'with open(os.environ["TUX_TEST_PACMAN_LOG"],"a") as f:'
            'f.write(json.dumps(sys.argv[1:])+"\\n")\n'
            'sys.exit(int(os.environ.get("TUX_TEST_PACMAN_FAIL","0")))\n'
        )
        pacman.chmod(0o755)
        self.environment = dict(os.environ, MSYSTEM="UCRT64",
                                PATH=str(binary) + os.pathsep + os.environ["PATH"],
                                TUX_TEST_PACMAN_LOG=str(self.events),
                                TUX_TEST_BUILD_MARKER=str(self.marker))
        self.environment.pop("TUX_UNATTENDED", None)

    def run_stage(self, stage, **environment):
        result = subprocess.run(
            ["bash", str(self.root / "packaging/auto-build-msys2.sh"), stage],
            cwd=self.root, env=dict(self.environment, **environment),
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=10,
        )
        return result

    def calls(self):
        return [json.loads(line) for line in self.events.read_text().splitlines()] if self.events.exists() else []

    def test_initialize_does_not_install_or_build(self):
        self.assertEqual(self.run_stage("initialize").returncode, 0)
        self.assertEqual(self.calls(), [])
        self.assertFalse(self.marker.exists())

    def test_update_is_noninteractive_and_separate_from_build(self):
        self.assertEqual(self.run_stage("update").returncode, 0)
        self.assertEqual(self.calls(), [["--noconfirm", "-Syu"]])
        self.assertFalse(self.marker.exists())

    def test_build_installs_required_dependencies_then_compiles(self):
        result = self.run_stage("build")
        self.assertEqual(result.returncode, 0, result.stdout)
        calls = self.calls()
        self.assertEqual(len(calls), 1)
        self.assertIn("--noconfirm", calls[0])
        self.assertIn("--needed", calls[0])
        for package in ("mingw-w64-ucrt-x86_64-gcc", "mingw-w64-ucrt-x86_64-qt6-base",
                        "mingw-w64-ucrt-x86_64-qt6-multimedia", "mingw-w64-ucrt-x86_64-libusb"):
            self.assertIn(package, calls[0])
        self.assertEqual(self.marker.read_text(), "built")
        self.assertTrue((self.root / "build-windows-logs/automatic-build.log").is_file())

    def test_failed_dependency_install_prevents_compilation(self):
        result = self.run_stage("build", TUX_TEST_PACMAN_FAIL="7")
        self.assertEqual(result.returncode, 7)
        self.assertFalse(self.marker.exists())
        self.assertNotIn("build tools are installed", result.stdout)

    def test_failed_update_is_reported_to_the_windows_controller(self):
        self.assertEqual(self.run_stage("update", TUX_TEST_PACMAN_FAIL="7").returncode, 7)
        self.assertFalse(self.marker.exists())

    def test_wrong_environment_does_not_touch_packages(self):
        result = self.run_stage("build", MSYSTEM="MINGW64")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("UCRT64", result.stdout)
        self.assertEqual(self.calls(), [])

    def test_unknown_stage_does_not_build(self):
        self.assertNotEqual(self.run_stage("unknown").returncode, 0)
        self.assertEqual(self.calls(), [])
        self.assertFalse(self.marker.exists())

    def test_manual_install_keeps_its_confirmation_prompt(self):
        result = subprocess.run(["bash", str(self.root / "packaging/install-windows-tools.sh")],
                                env=self.environment, text=True, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("--noconfirm", self.calls()[0])


if __name__ == "__main__":
    unittest.main()
