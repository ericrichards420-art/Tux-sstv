"""Check the Hamlib header repair and failed-build resume on a Linux host.

The tiny C fixture models the reported missing-header error. It is not the
Hamlib source or a native Windows build. Native build tools are stand-ins in
the orchestration tests; SHA-256 verification and tar extraction are real.
"""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[1]
PATCH = ROOT / "packaging/patch-hamlib-windows.sh"
FIXTURE = """#ifdef _WIN32
#include <windows.h>
static void init_pfFreq(void) {
    int has_qpc = 1;
    assert(has_qpc);
}
int main(void) { init_pfFreq(); return 0; }
#endif
"""


class HamlibRepairTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tux-hamlib-repair-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "source ' & spaces"
        (self.source / "lib").mkdir(parents=True)
        self.timer = self.source / "lib/precise_time.c"
        self.timer.write_text(FIXTURE)

    def patch(self, source=None):
        return subprocess.run(["bash", str(PATCH), str(source or self.source)],
                              text=True, capture_output=True, timeout=10)

    def test_reported_c_error_is_fixed_without_disabling_assertions(self):
        compiler = shutil.which("gcc")
        self.assertIsNotNone(compiler, "Host GCC is required for this fixture check")
        (self.source / "windows.h").write_text("/* No Windows APIs in this fixture. */\n")
        executable = self.root / "probe"
        command = [compiler, "-D_WIN32", "-Werror=implicit-function-declaration",
                   "-I", str(self.source), str(self.timer), "-o", str(executable)]
        before = subprocess.run(command, text=True, capture_output=True, timeout=20)
        self.assertNotEqual(before.returncode, 0)
        self.assertIn("assert", before.stderr)
        result = self.patch()
        self.assertEqual(result.returncode, 0, result.stderr)
        after = subprocess.run(command, text=True, capture_output=True, timeout=20)
        self.assertEqual(after.returncode, 0, after.stderr)
        subprocess.run([str(executable)], check=True, timeout=10)
        # A false assertion must still abort; this is not an error suppression.
        self.timer.write_text(self.timer.read_text().replace("has_qpc = 1", "has_qpc = 0"))
        subprocess.run(command, check=True, capture_output=True, timeout=20)
        def no_core_dump():
            import resource
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        failed_assertion = subprocess.run([str(executable)], capture_output=True,
                                          preexec_fn=no_core_dump, timeout=10)
        self.assertNotEqual(failed_assertion.returncode, 0)
        self.assertIn(b"has_qpc", failed_assertion.stderr)

    def test_repeated_patch_keeps_identical_bytes(self):
        self.assertEqual(self.patch().returncode, 0)
        first = self.timer.read_bytes()
        result = self.patch()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.timer.read_bytes(), first)
        self.assertEqual(first.count(b"#include <assert.h>"), 1)

    def test_other_conditional_assert_header_does_not_hide_windows_error(self):
        self.timer.write_text("#ifndef _WIN32\n#include <assert.h>\n#endif\n" + FIXTURE)
        self.assertEqual(self.patch().returncode, 0)
        self.assertIn("#include <windows.h>\n#include <assert.h>", self.timer.read_text())

    def test_missing_source_fails(self):
        result = self.patch(self.root / "missing")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing", result.stderr)

    def test_unexpected_source_context_is_not_modified(self):
        for text in (FIXTURE.replace("<windows.h>", "<other.h>"),
                     FIXTURE.replace("assert(has_qpc)", "assert(other)"),
                     FIXTURE + "#include <windows.h>\n"):
            with self.subTest(source=text):
                self.timer.write_text(text)
                self.assertNotEqual(self.patch().returncode, 0)
                self.assertEqual(self.timer.read_text(), text)


class HamlibBuildResumeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tux-hamlib-build-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.repo = self.root / "kit ' & spaces"
        packaging = self.repo / "packaging"
        packaging.mkdir(parents=True)
        shutil.copy2(PATCH, packaging / PATCH.name)
        shutil.copy2(ROOT / 'packaging/normalize-windows-source-times.sh', packaging)
        (self.repo / "CMakeLists.txt").write_text("project(TuxSSTV VERSION 1.15.0 LANGUAGES CXX)\n")
        self.cache = self.root / "cache"
        seed = self.cache / "seed/hamlib-4.7.2"
        (seed / "lib").mkdir(parents=True)
        (seed / "lib/precise_time.c").write_text(FIXTURE)
        configure = seed / "configure"
        configure.write_text("""#!/usr/bin/env bash
set -euo pipefail
grep -q 'Tux-sstv: Hamlib 4.7.2 Windows assert fix' "$(dirname -- "$0")/lib/precise_time.c"
echo configure >> "$TUX_TEST_EVENTS"
""")
        configure.chmod(0o755)
        self.archive = self.cache / "hamlib-4.7.2.tar.gz"
        with tarfile.open(self.archive, "w:gz") as archive:
            archive.add(seed, arcname="hamlib-4.7.2")
        # Only the test copy uses a synthetic source archive and its real hash.
        builder = (ROOT / "packaging/build-windows.sh").read_text()
        pinned = "ae1fcf2dbc80ea0786ea8f047b09399c3f7737d1930442f61a031708ed33e88f"
        self.assertEqual(builder.count(pinned), 1)
        (packaging / "build-windows.sh").write_text(
            builder.replace(pinned, hashlib.sha256(self.archive.read_bytes()).hexdigest()))
        binary = self.root / "bin"
        binary.mkdir()
        programs = {
            "cmake": 'echo application-configure >> "$TUX_TEST_EVENTS"; exit 73',
            "curl": 'echo unexpected-download >> "$TUX_TEST_EVENTS"; exit 91',
            "ninja": "exit 92", "windeployqt": "exit 92", "objdump": "exit 92",
            "cygpath": 'printf "%s\\n" "$2"',
            "make": """echo "make $*" >> "$TUX_TEST_EVENTS"
if [[ ${TUX_TEST_FAIL_MAKE:-0} == 1 ]]; then exit 7; fi
if [[ ${1:-} == install ]]; then
    mkdir -p "$TUX_HAMLIB_CACHE/install/include/hamlib" "$TUX_HAMLIB_CACHE/install/bin"
    touch "$TUX_HAMLIB_CACHE/install/include/hamlib/rig.h" "$TUX_HAMLIB_CACHE/install/bin/rigctl.exe"
fi""",
        }
        for name, script in programs.items():
            program = binary / name
            program.write_text("#!/usr/bin/env bash\nset -euo pipefail\n" + script + "\n")
            program.chmod(0o755)
        windows = self.root / 'Windows'
        powershell = windows / 'System32/WindowsPowerShell/v1.0/powershell.exe'
        powershell.parent.mkdir(parents=True)
        powershell.write_text('#!/usr/bin/env bash\nexit 93\n')
        powershell.chmod(0o755)
        self.events = self.root / "events"
        self.environment = dict(os.environ, MSYSTEM="UCRT64", TUX_HAMLIB_CACHE=str(self.cache),
                                SYSTEMROOT=str(windows),
                                TUX_BUILD_JOBS="4",
                                TUX_TEST_EVENTS=str(self.events),
                                PATH=str(binary) + os.pathsep + os.environ["PATH"])

    def build(self, **environment):
        return subprocess.run(["bash", str(self.repo / "packaging/build-windows.sh")],
                              env=dict(self.environment, **environment), text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)

    def test_failed_make_resumes_with_verified_cached_download_and_header_fix(self):
        original_archive = self.archive.read_bytes()
        first = self.build(TUX_TEST_FAIL_MAKE="1")
        self.assertEqual(first.returncode, 7, first.stdout)
        self.assertNotIn("application-configure", self.events.read_text())
        second = self.build()
        self.assertEqual(second.returncode, 73, second.stdout)  # Stand-in CMake stops here.
        self.assertIn("Applied Hamlib 4.7.2 Windows assertion header fix", second.stdout)
        calls = self.events.read_text().splitlines()
        self.assertEqual(calls, ["configure", "make -j4", "configure", "make -j4",
                                 "make install", "application-configure"])
        self.assertEqual(self.archive.read_bytes(), original_archive)
        # A completed Hamlib installation is reused on subsequent application retries.
        third = self.build()
        self.assertEqual(third.returncode, 73, third.stdout)
        self.assertEqual(self.events.read_text().splitlines(), calls + ["application-configure"])

    def test_corrupt_cached_download_fails_before_patching_or_compilation(self):
        self.archive.write_bytes(self.archive.read_bytes() + b"corrupt")
        result = self.build()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("FAILED", result.stdout)
        self.assertFalse(self.events.exists())
        self.assertFalse((self.cache / "hamlib-4.7.2").exists())

    def test_failed_test_prints_assertion_at_end_and_prevents_packaging(self):
        binary = self.root / "bin"
        (binary / "cmake").write_text('#!/usr/bin/env bash\nexit 0\n')
        ctest = binary / "ctest"
        ctest.write_text('''#!/usr/bin/env bash
printf 'FAIL! : CodecTests::example() required assertion failed\\n   Loc: fixture.cpp(10)\\n'
for ((n=0;n<100;n++)); do echo 'PASS : another check'; done
exit 8
''')
        ctest.chmod(0o755)
        result = self.build()
        self.assertEqual(result.returncode, 8, result.stdout)
        details = result.stdout.split("Failed test details", 1)[1]
        self.assertIn("FAIL! : CodecTests::example()", details)
        self.assertIn("Loc: fixture.cpp(10)", details)
        self.assertFalse((self.repo / "dist").exists())

    def test_future_source_timestamp_is_repaired_before_cmake(self):
        cmake_file = self.repo / 'CMakeLists.txt'
        future = time.time() + 7200
        os.utime(cmake_file, (future, future))
        (self.root / 'bin/cmake').write_text('''#!/usr/bin/env python3
from pathlib import Path
import sys,time
sys.exit(73 if Path('CMakeLists.txt').stat().st_mtime <= time.time() else 85)
''')
        result = self.build()
        self.assertEqual(result.returncode, 73, result.stdout)
        self.assertLessEqual(cmake_file.stat().st_mtime, time.time())
        self.assertIn('corrected 1 future-dated', result.stdout)

    def test_manifest_failure_keeps_exit_code_and_collects_only_dry_run(self):
        binary = self.root / 'bin'
        (binary / 'cmake').write_text('''#!/usr/bin/env bash
if [[ ${1:-} == --build ]]; then
    echo "ninja: error: manifest 'build.ninja' still dirty after 100 tries"
    exit 1
fi
exit 0
''')
        (binary / 'ninja').write_text('''#!/usr/bin/env bash
echo "ninja $*" >> "$TUX_TEST_EVENTS"
echo 'ninja explain: example future dependency'
''')
        result = self.build()
        self.assertEqual(result.returncode, 1, result.stdout)
        self.assertIn('ninja explain: example future dependency', result.stdout)
        self.assertIn('ninja -C build-windows -d explain -n', self.events.read_text())
        self.assertFalse((self.repo / 'dist').exists())


if __name__ == "__main__":
    unittest.main()
