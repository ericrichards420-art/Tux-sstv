"""Exercise source timestamp repair and real CMake/Ninja regeneration on Linux."""
import os
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
REPAIR = ROOT / 'packaging/normalize-windows-source-times.sh'


class TimestampTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tux-time ' & ")
        self.addCleanup(self.temporary.cleanup)
        self.source = Path(self.temporary.name) / 'source'
        self.source.mkdir()
        (self.source / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.22)\nproject(TuxTimeProbe NONE)\n'
            'add_custom_target(probe ALL COMMAND "${CMAKE_COMMAND}" -E touch '
            '"${CMAKE_BINARY_DIR}/probe-complete")\n')
        self.future = time.time_ns() + 2 * 3600 * 1_000_000_000

    def future_file(self, name):
        file = self.source / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(b'unchanged test contents\n')
        os.utime(file, ns=(self.future, self.future))
        return file

    def repair(self, folder=None):
        return subprocess.run(['bash', str(REPAIR), str(folder or self.source)],
                              text=True, capture_output=True, timeout=10)

    def test_only_future_source_mtimes_change_and_repeat_is_noop(self):
        future = self.future_file('resources/test \n quote\' & file.qrc')
        future.chmod(0o640)
        old = self.source / 'CMakeLists.txt'
        old_ns = time.time_ns() - 3600 * 1_000_000_000
        os.utime(old, ns=(old_ns, old_ns))
        contents = future.read_bytes()
        mode = future.stat().st_mode
        result = self.repair()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('corrected 1 future-dated', result.stdout)
        self.assertLessEqual(future.stat().st_mtime_ns, time.time_ns())
        self.assertEqual(future.read_bytes(), contents)
        self.assertEqual(future.stat().st_mode, mode)
        self.assertEqual(old.stat().st_mtime_ns, old_ns)
        self.assertIn('corrected 0 future-dated', self.repair().stdout)
        self.assertEqual(list((self.source / 'build-windows-logs').iterdir()), [])

    def test_generated_files_and_external_symlinks_are_untouched(self):
        excluded = [self.future_file(name) for name in (
            'build-windows/build.ninja', 'build-windows-logs/build.log',
            'build-evidence/proof.log', 'dist/bundle/file', '.git/config',
            'tests/__pycache__/cache')]
        external = self.source.parent / 'outside.cmake'
        external.write_text('outside')
        os.utime(external, ns=(self.future, self.future))
        (self.source / 'linked.cmake').symlink_to(external)
        (self.source / 'linked-dir').symlink_to(self.source.parent, target_is_directory=True)
        result = self.repair()
        self.assertEqual(result.returncode, 0, result.stderr)
        for file in excluded + [external]:
            self.assertEqual(file.stat().st_mtime_ns, self.future, file)

    def test_invalid_root_is_rejected(self):
        self.assertNotEqual(self.repair(self.source.parent).returncode, 0)

    def test_updated_archive_refreshes_inputs_once_for_existing_build(self):
        manifest=self.source/'SOURCE_SHA256.json';manifest.write_text('{"revision":1}\n')
        build=self.source/'build-windows';build.mkdir();(build/'CMakeCache.txt').write_text('existing build')
        source=self.source/'CMakeLists.txt';old=time.time_ns()-3600*1_000_000_000
        os.utime(source,ns=(old,old));contents=source.read_bytes()
        first=self.repair();self.assertEqual(first.returncode,0,first.stderr)
        self.assertIn('Updated source kit detected',first.stdout)
        self.assertGreater(source.stat().st_mtime_ns,old)
        stamp=build/'.tux-source-archive.sha256'
        self.assertEqual(stamp.read_text().strip(),hashlib.sha256(manifest.read_bytes()).hexdigest())
        refreshed=source.stat().st_mtime_ns
        self.assertEqual(self.repair().returncode,0)
        self.assertEqual(source.stat().st_mtime_ns,refreshed)
        manifest.write_text('{"revision":2}\n');os.utime(source,ns=(old,old))
        self.assertIn('Updated source kit detected',self.repair().stdout)
        self.assertGreater(source.stat().st_mtime_ns,old)
        self.assertEqual(source.read_bytes(),contents)

    def test_fresh_archive_records_identity_without_redating_old_inputs(self):
        (self.source/'SOURCE_SHA256.json').write_text('{}\n')
        source=self.source/'CMakeLists.txt';old=time.time_ns()-3600*1_000_000_000
        os.utime(source,ns=(old,old))
        result=self.repair();self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(source.stat().st_mtime_ns,old)
        self.assertTrue((self.source/'build-windows/.tux-source-archive.sha256').is_file())

    @unittest.skipUnless(shutil.which('cmake') and shutil.which('ninja'),
                         'Real CMake and Ninja are required for regeneration fixture')
    def test_actual_ninja_future_manifest_loop_is_fixed_and_regeneration_kept(self):
        cmake_file = self.source / 'CMakeLists.txt'
        os.utime(cmake_file, ns=(self.future, self.future))
        build = self.source / 'build-windows'
        configure = ['cmake', '-S', str(self.source), '-B', str(build), '-G', 'Ninja']
        subprocess.run(configure, check=True, capture_output=True, timeout=20)
        before = subprocess.run(['ninja', '-C', str(build)], text=True,
                                capture_output=True, timeout=60)
        self.assertNotEqual(before.returncode, 0)
        self.assertIn('still dirty after 100 tries', before.stdout + before.stderr)
        repaired = self.repair()
        self.assertEqual(repaired.returncode, 0, repaired.stderr)
        subprocess.run(configure, check=True, capture_output=True, timeout=20)
        after = subprocess.run(['ninja', '-C', str(build)], text=True,
                               capture_output=True, timeout=20)
        self.assertEqual(after.returncode, 0, after.stdout + after.stderr)
        self.assertTrue((build / 'probe-complete').exists())
        # Future edits must still trigger normal CMake regeneration.
        cmake_file.write_text(cmake_file.read_text() + 'message(STATUS "Normal regeneration works")\n')
        again = subprocess.run(['ninja', '-C', str(build)], text=True,
                               capture_output=True, timeout=20)
        self.assertEqual(again.returncode, 0, again.stdout + again.stderr)
        self.assertIn('Normal regeneration works', again.stdout)


if __name__ == '__main__':
    unittest.main()
