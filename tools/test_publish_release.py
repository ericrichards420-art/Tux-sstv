import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("release", Path(__file__).with_name("publish-release.py"))
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseChecks(unittest.TestCase):
    def test_rejects_missing_extra_corrupt_and_truncated_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "package.deb"
            path.write_bytes(b"package data")
            asset = {"name": path.name, "size": path.stat().st_size, "digest": "sha256:" + release.digest(path)}
            release.verify_assets([asset], {path.name: path})
            for assets in ([], [dict(asset, digest="sha256:bad")], [dict(asset, size=0)],
                           [asset, dict(asset, name="extra.exe")]):
                with self.assertRaises(RuntimeError):
                    release.verify_assets(assets, {path.name: path})

    def test_debian_asset_normalization(self):
        self.assertEqual(release.asset_name("tux_1~ubuntu.deb"), "tux_1.ubuntu.deb")

    def test_source_archive_is_repeatable_and_has_license_and_manifest(self):
        import zipfile
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "windows").mkdir()
            for name in ("LICENSE.md", "CMakeLists.txt"):
                (root / "windows" / name).write_text(name)
            with patch.object(release, "ROOT", root), patch.object(release.subprocess, "check_output",
                    return_value=b"windows/LICENSE.md\0windows/CMakeLists.txt\0"):
                release.source_archive("windows", root / "a.zip")
                release.source_archive("windows", root / "b.zip")
            self.assertEqual(release.digest(root / "a.zip"), release.digest(root / "b.zip"))
            with zipfile.ZipFile(root / "a.zip") as archive:
                manifest = json.loads(archive.read("Tux-sstv-Windows/SOURCE_SHA256.json"))
                self.assertIn("LICENSE.md", manifest)

    def check_existing(self, different_source=False, broken_assets=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "linux").mkdir()
            (root / "linux/CMakeLists.txt").write_text("project(TuxSSTV VERSION 1.15.0 LANGUAGES CXX)")
            config = {"version": "1.15.0", "tag": "linux-v1.15.0", "patterns": ["program.deb"], "source_archive": "source.zip"}
            (root / "release.json").write_text(json.dumps({"linux": config}))
            assets = [{"name": name, "size": 50, "digest": "sha256:" + "c" * 64}
                      for name in ("program.deb", "source.zip", "SHA256SUMS.txt")]
            if broken_assets:
                assets.pop()
            def get(path):
                if "/git/ref/" in path:
                    return {"object": {"type": "commit", "sha": "b" * 40}}
                return {"id": 123, "draft": False, "html_url": "https://github.com/example/release"}
            with patch.object(release, "ROOT", root), patch.object(release.sys, "argv", ["publish", "linux", "unused"]), \
                    patch.dict(os.environ, GITHUB_REPOSITORY=release.REPOSITORY, GITHUB_SHA="a" * 40), \
                    patch.object(release, "maybe_get", side_effect=get), \
                    patch.object(release, "platform_tree", side_effect=["old", "new"] if different_source else ["same", "same"]), \
                    patch.object(release, "api", return_value=assets) as api:
                release.main()
                # A published release is never uploaded to or edited on a rerun.
                self.assertTrue(all(len(call.args) == 1 for call in api.call_args_list))

    def test_preserves_release_after_other_platform_changes(self):
        self.check_existing()

    def test_rejects_reusing_a_version_for_changed_platform_source(self):
        with self.assertRaisesRegex(RuntimeError, "bump its version"):
            self.check_existing(different_source=True)

    def test_rejects_incomplete_published_release(self):
        with self.assertRaisesRegex(RuntimeError, "incomplete assets"):
            self.check_existing(broken_assets=True)


if __name__ == "__main__":
    unittest.main()
