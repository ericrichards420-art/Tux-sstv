#!/usr/bin/env python3
"""Publish only the packages produced by a successful platform workflow job."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
import zipfile

REPOSITORY = "ericrichards420-art/Tux-sstv"
ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def asset_name(name):
    # GitHub normalizes the Debian revision separator in release asset names.
    return name.replace("~", ".")


def source_archive(platform, target):
    files = subprocess.check_output(
        ["git", "ls-files", "-z", "--", platform], cwd=ROOT
    ).decode().split("\0")
    prefix = "Tux-sstv-Windows" if platform == "windows" else "Tux-sstv-Linux"
    hashes = {}
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name in sorted(filter(None, files)):
            path = ROOT / name
            if path.is_symlink() or not path.is_file():
                raise RuntimeError(f"Unexpected source entry: {name}")
            relative = Path(name).relative_to(platform).as_posix()
            data = path.read_bytes()
            hashes[relative] = hashlib.sha256(data).hexdigest()
            info = zipfile.ZipInfo(f"{prefix}/{relative}", (2000, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = path.stat().st_mode << 16
            archive.writestr(info, data)
        info = zipfile.ZipInfo(f"{prefix}/SOURCE_SHA256.json", (2000, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        archive.writestr(info, json.dumps(hashes, indent=2) + "\n")
    if "LICENSE.md" not in hashes or "CMakeLists.txt" not in hashes:
        raise RuntimeError("Incomplete source archive")


def api(path, method="GET", body=None, binary=None):
    host = "https://uploads.github.com" if binary else "https://api.github.com"
    headers = {
        "Authorization": f"Bearer {os.environ['GH_TOKEN']}",
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "Tux-sstv-release",
    }
    data = None
    if body is not None:
        headers["Content-Type"] = "application/json"
        data = json.dumps(body).encode()
    if binary:
        headers["Content-Type"] = "application/octet-stream"
        data = binary.read_bytes()
    request = urllib.request.Request(host + path, data=data, headers=headers, method=method)
    with urllib.request.urlopen(request, timeout=300) as response:
        return json.load(response)


def maybe_get(path):
    try:
        return api(path)
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return None
        raise


def verify_assets(assets, expected):
    actual = {entry["name"]: entry for entry in assets}
    if len(actual) != len(assets) or actual.keys() != expected.keys():
        raise RuntimeError("Release assets do not exactly match this platform's packages")
    for name, path in expected.items():
        entry = actual[name]
        if entry["size"] != path.stat().st_size or entry.get("digest") != "sha256:" + digest(path):
            raise RuntimeError(f"Release asset size or SHA-256 mismatch: {name}")


def platform_tree(base, commit, platform):
    tree = api(base + "/commits/" + commit)["commit"]["tree"]["sha"]
    entries = api(base + "/git/trees/" + tree)["tree"]
    return next(entry["sha"] for entry in entries if entry["path"] == platform and entry["type"] == "tree")


def main():
    platform, output_arg = sys.argv[1:]
    config = json.loads((ROOT / "release.json").read_text())[platform]
    if os.environ.get("GITHUB_REPOSITORY") != REPOSITORY:
        raise RuntimeError("This release workflow is only for the original developer's repository")
    commit = os.environ["GITHUB_SHA"]
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise RuntimeError("Expected the exact workflow commit SHA")
    cmake = (ROOT / platform / "CMakeLists.txt").read_text()
    version = re.search(r"project\(TuxSSTV VERSION ([0-9.]+)", cmake).group(1)
    if version != config["version"]:
        raise RuntimeError("Release version does not match the built source")
    base = f"/repos/{REPOSITORY}"
    tag = config["tag"]
    ref = maybe_get(base + "/git/ref/tags/" + urllib.parse.quote(tag, safe=""))
    release = maybe_get(base + "/releases/tags/" + urllib.parse.quote(tag, safe=""))
    if release and not release["draft"]:
        if not ref or ref["object"]["type"] != "commit":
            raise RuntimeError("Published release is missing its expected source tag")
        original = ref["object"]["sha"]
        if original != commit and platform_tree(base, original, platform) != platform_tree(base, commit, platform):
            raise RuntimeError("This platform changed after publication; bump its version before releasing")
        assets = api(base + f"/releases/{release['id']}/assets?per_page=100")
        names = {asset_name(name) for name in config["patterns"]} | {config["source_archive"], "SHA256SUMS.txt"}
        if {entry["name"] for entry in assets} != names or any(
                entry["size"] <= 0 or not re.fullmatch(r"sha256:[0-9a-f]{64}", entry.get("digest") or "") for entry in assets):
            raise RuntimeError("Existing published release has incomplete assets")
        print("Preserved verified release with identical platform source:", release["html_url"])
        return
    output = Path(output_arg).resolve()
    expected = {}
    for pattern in config["patterns"]:
        matches = list(output.glob(pattern))
        if len(matches) != 1 or matches[0].stat().st_size < 1_000_000:
            raise RuntimeError(f"Missing or invalid built package: {pattern}")
        expected[asset_name(matches[0].name)] = matches[0]
    archive = output / config["source_archive"]
    source_archive(platform, archive)
    expected[archive.name] = archive
    checksums = output / "SHA256SUMS.txt"
    checksums.write_text("".join(f"{digest(path)}  {name}\n" for name, path in sorted(expected.items())))
    expected[checksums.name] = checksums
    if ref and (ref["object"]["type"] != "commit" or ref["object"]["sha"] != commit):
        raise RuntimeError("The version tag already points to different source; it was preserved")
    if release is None:
        notes = (
            "Developed by **Eric Richards, KI5BZO**.\n\n"
            "### Guided radio setup\n"
            "- Separate audio and CAT choices for SignaLink and radio USB interfaces.\n"
            "- Explicit CAT, RTS/DTR, audio-operated, and manual PTT choices.\n"
            "- Flex SmartSDR CAT virtual COM and native SmartSDR network presets.\n"
            "- Separate DAX receive/transmit devices, configuration review, connection check, "
            "and an optional one-second PTT test.\n\n"
            f"Built and tested by the native {platform} workflow from source commit `{commit}`. "
            "Automated checks do not replace Flex hardware or on-air testing.\n\n"
            "Download the `.deb` for **Ubuntu 26.04 amd64**, or the `-setup.exe` for "
            "**Windows 10 1809+ / Windows 11 x64**, according to this release's platform. "
            "The Windows preview ZIP is the portable application; `tux-sstv-windows.zip` "
            "is the source and one-click build kit.\n\n"
            f"[Setup guide](https://github.com/{REPOSITORY}/blob/{commit}/{platform}/docs/SETUP_GUIDE.md) · "
            f"[Corresponding source](https://github.com/{REPOSITORY}/tree/{commit}/{platform})\n\n"
            "Project source is provided under the Tux-sstv Personal Compatibility Source License. "
            "Keep the original developer credit; contact ericrichards420@gmail.com for improvements "
            "or redistribution permission. Third-party component licenses apply independently.\n"
        )
        release = api(base + "/releases", "POST", {
            "tag_name": tag, "target_commitish": commit, "name": config["title"],
            "body": notes, "draft": True, "prerelease": False,
        })
    if release["target_commitish"] != commit:
        raise RuntimeError("Existing draft belongs to different source; it was preserved")
    existing = {entry["name"]: entry for entry in api(base + f"/releases/{release['id']}/assets?per_page=100")}
    if not existing.keys() <= expected.keys():
        raise RuntimeError("Draft contains unrelated assets; they were preserved")
    for name, path in expected.items():
        if name in existing:
            verify_assets([existing[name]], {name: path})
            continue
        print("Uploading", name, flush=True)
        api(base + f"/releases/{release['id']}/assets?name=" + urllib.parse.quote(name, safe=""),
            "POST", binary=path)
    verify_assets(api(base + f"/releases/{release['id']}/assets?per_page=100"), expected)
    published = api(base + f"/releases/{release['id']}", "PATCH", {
        "draft": False, "make_latest": "true" if config["latest"] else "false",
    })
    if published["draft"]:
        raise RuntimeError("GitHub left the release as a draft")
    print("Published and verified:", published["html_url"])


if __name__ == "__main__":
    main()
