"""Exercise real deployment scripts with fake DLLs/executables, not Windows APIs."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PlatformPackagingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tux-platform ' & ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.stage = self.root / 'portable app'
        self.stage.mkdir()
        (self.stage / '.tux-windows-staging').touch()
        self.plugins = self.root / 'Qt plugins'
        (self.plugins / 'platforms').mkdir(parents=True)
        for name in ('qwindows', 'qoffscreen'):
            (self.plugins / 'platforms' / (name + '.dll')).write_bytes((name + ' fixture').encode())
        self.logs = self.root / 'logs'
        self.calls = self.root / 'calls.jsonl'
        binary = self.root / 'bin'
        binary.mkdir()
        converter = binary / 'cygpath'
        converter.write_text('#!/usr/bin/env bash\nprintf "%s\\n" "$2"\n')
        converter.chmod(0o755)
        unavailable_cmp = binary / 'cmp'
        unavailable_cmp.write_text('#!/usr/bin/env bash\nprintf used > "$TUX_TEST_CMP_MARKER"\nexit 127\n')
        unavailable_cmp.chmod(0o755)
        self.cmp_marker = self.root / 'cmp-was-used'
        self.environment = dict(os.environ, PATH=str(binary) + os.pathsep + os.environ['PATH'],
                                TUX_TEST_CMP_MARKER=str(self.cmp_marker),
                                TUX_TEST_CALLS=str(self.calls), TUX_TEST_STAGE=str(self.stage),
                                QT_QPA_PLATFORM='windows', QT_PLUGIN_PATH='old unrelated Qt',
                                QT_QPA_PLATFORM_PLUGIN_PATH='old platform path')
        app = self.stage / 'tux-sstv.exe'
        app.write_text('''#!/usr/bin/env python3
import json,os,sys
from pathlib import Path
assert os.environ['QT_QPA_PLATFORM']=='offscreen'
assert 'QT_PLUGIN_PATH' not in os.environ
assert os.environ['QT_QPA_PLATFORM_PLUGIN_PATH']==str(Path(os.environ['TUX_TEST_STAGE'])/'platforms')
assert os.environ['QT_DEBUG_PLUGINS']=='1' and os.environ['QT_LOGGING_TO_CONSOLE']=='1'
with open(os.environ['TUX_TEST_CALLS'],'a') as output: output.write(json.dumps(sys.argv[1:])+'\\n')
if os.environ.get('TUX_TEST_APP_FAIL'):
    print('Qt plugin diagnostic fixture',file=sys.stderr);sys.exit(12)
if sys.argv[1:]==['--version']: print('Tux-sstv 1.15.0')
elif sys.argv[1:]==['--list-modes']: print(json.dumps([{'id':str(n)} for n in range(43)]))
else: sys.exit(91)
''')
        app.chmod(0o755)
        rig = self.stage / 'rigctl.exe'
        rig.write_text('#!/usr/bin/env python3\nprint("Model catalog")\nfor n in range(101): print(" %d Fixture radio"%n)\n')
        rig.chmod(0o755)

    def deploy(self):
        return subprocess.run(['bash', str(ROOT / 'packaging/deploy-windows-platforms.sh'),
                               str(self.stage), str(self.plugins)],
                              env=self.environment, capture_output=True, text=True, timeout=10)

    def check(self, **environment):
        return subprocess.run(['bash', str(ROOT / 'packaging/check-windows-stage.sh'),
                               str(self.stage), str(self.logs)],
                              env=dict(self.environment, **environment),
                              capture_output=True, text=True, timeout=10)

    def test_deploys_both_platforms_and_portable_paths(self):
        result = self.deploy()
        self.assertEqual(result.returncode, 0, result.stderr)
        for name in ('qwindows.dll', 'qoffscreen.dll'):
            self.assertEqual((self.stage/'platforms'/name).read_bytes(),
                             (self.plugins/'platforms'/name).read_bytes())
        self.assertEqual((self.stage/'qt.conf').read_text(), '[Paths]\nPrefix=.\nPlugins=.\n')

    def test_repeated_deploy_does_not_replace_matching_plugins(self):
        self.assertEqual(self.deploy().returncode, 0)
        plugin = self.stage/'platforms/qoffscreen.dll'
        stamp = plugin.stat().st_mtime_ns
        self.assertEqual(self.deploy().returncode, 0)
        self.assertEqual(plugin.stat().st_mtime_ns, stamp)

    def test_missing_offscreen_fails_before_writing_bundle(self):
        (self.plugins/'platforms/qoffscreen.dll').unlink()
        result = self.deploy()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('qoffscreen.dll', result.stderr)
        self.assertFalse((self.stage/'platforms').exists())

    def test_mixed_qt_plugin_is_not_overwritten_or_shipped(self):
        (self.stage/'platforms').mkdir()
        target = self.stage/'platforms/qwindows.dll'
        target.write_bytes(b'different Qt build')
        self.assertNotEqual(self.deploy().returncode, 0)
        self.assertEqual(target.read_bytes(), b'different Qt build')
        self.assertFalse((self.stage/'qt.conf').exists())

    def test_requires_owned_staging_folder(self):
        (self.stage/'.tux-windows-staging').unlink()
        self.assertNotEqual(self.deploy().returncode, 0)
        self.assertFalse((self.stage/'qt.conf').exists())

    def test_matching_installed_plugin_is_checked_without_cmp(self):
        (self.stage/'platforms').mkdir()
        (self.stage/'platforms/qwindows.dll').write_bytes((self.plugins/'platforms/qwindows.dll').read_bytes())
        result = self.deploy()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(self.cmp_marker.exists())
        self.assertTrue((self.stage/'platforms/qoffscreen.dll').is_file())

    def test_hash_read_error_is_not_reported_as_plugin_mismatch(self):
        self.assertEqual(self.deploy().returncode, 0)
        tool = self.root/'bin/sha256sum'
        tool.write_text('#!/usr/bin/env bash\necho "Fixture read error" >&2\nexit 42\n')
        tool.chmod(0o755)
        result = self.deploy()
        self.assertEqual(result.returncode, 42)
        self.assertIn('Fixture read error', result.stderr)
        self.assertNotIn('differs from', result.stderr)

    def test_checks_force_bundled_offscreen_and_only_metadata_commands(self):
        self.assertEqual(self.deploy().returncode, 0)
        result = self.check()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual([json.loads(line) for line in self.calls.read_text().splitlines()],
                         [['--version'], ['--list-modes']])
        self.assertEqual((self.logs/'staged-version.txt').read_text().strip(), 'Tux-sstv 1.15.0')
        self.assertEqual(len(json.loads((self.logs/'staged-modes.json').read_text())), 43)

    def test_missing_bundled_plugin_is_rejected_before_startup(self):
        self.assertEqual(self.deploy().returncode, 0)
        (self.stage/'platforms/qoffscreen.dll').unlink()
        result = self.check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing bundle component before startup: platforms/qoffscreen.dll', result.stderr)
        self.assertFalse(self.calls.exists())

    def test_failed_startup_preserves_exit_code_and_prints_qt_details(self):
        self.assertEqual(self.deploy().returncode, 0)
        result = self.check(TUX_TEST_APP_FAIL='1')
        self.assertEqual(result.returncode, 12)
        self.assertIn('Qt plugin diagnostic fixture', result.stderr)
        self.assertIn('Qt plugin diagnostic fixture', (self.logs/'staged-version-plugins.log').read_text())
        self.assertEqual(len(self.calls.read_text().splitlines()), 1)

    def validator(self):
        return subprocess.run(['cmake', '-DSTAGE='+str(self.stage), '-DLOG_DIR='+str(self.logs),
                               '-DAPP_VERSION=1.15.0', '-P', str(ROOT/'cmake/VerifyWindowsBundle.cmake')],
                              capture_output=True, text=True, timeout=10)

    def complete_fixture(self):
        self.assertEqual(self.deploy().returncode, 0)
        self.assertEqual(self.check().returncode, 0)
        (self.stage/'tux-sstv-audio.exe').touch()
        (self.stage/'multimedia').mkdir()
        (self.stage/'multimedia/fixture.dll').touch()

    def test_complete_presence_metadata_fixture_is_valid(self):
        self.complete_fixture()
        result = self.validator()
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)

    def test_bundle_validator_rejects_windows_only_platforms(self):
        self.complete_fixture()
        (self.stage/'platforms/qoffscreen.dll').unlink()
        result = self.validator()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing bundle component: platforms/qoffscreen.dll', result.stderr)


if __name__ == '__main__':
    unittest.main()
