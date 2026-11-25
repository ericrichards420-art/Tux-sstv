# Building Tux-sstv from the published source

Original project developer: **Eric Richards, KI5BZO**
Questions and improvement proposals: **ericrichards420@gmail.com**

The two directories preserve the current platform releases:

| Directory | Application version | Existing package target |
| --- | --- | --- |
| `linux/` | 1.14.3 | Ubuntu 26.04, amd64 |
| `windows/` | 1.15.3 | Windows 10 1809+ or Windows 11, x64 |

Read [LICENSE.md](LICENSE.md) before making a compatibility build. The source
permission is for personal use only; redistribution requires Eric's separate
written permission.
Keep Eric Richards's original developer credit and all applicable third-party
notices. Propose feature changes to Eric at the address above.

## Linux

From the repository root, install the development dependencies appropriate to
your Linux distribution. On the existing Ubuntu target these include:

```bash
sudo apt install build-essential cmake qt6-base-dev qt6-qpa-plugins qt6-svg-plugins libhamlib-dev libhamlib-utils alsa-utils python3 fonts-noto-color-emoji gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good
```

Configure and build:

```bash
cmake -S linux -B linux/build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build linux/build -j 4
QT_QPA_PLATFORM=offscreen ctest --test-dir linux/build --output-on-failure
./linux/build/tux-sstv
```

The existing Debian packaging command is:

```bash
./linux/packaging/build-deb.sh
```

Build a package on the distribution and architecture it is intended for, using
matching development and runtime libraries. A package built on Ubuntu 26.04 is
not a universal package for other Linux versions. See the Linux README and
`linux/docs/QUICKSTART.md` for the runtime and station setup details.

## Windows

Open the `windows` folder and run **Install and Build Tux-sstv.cmd**. The existing
bootstrap installs its build tools, builds and tests the application, and makes
the Windows installer. Its detailed instructions and supported dependency
versions are in `windows/WINDOWS-START-HERE.txt`.

For a new Windows compatibility target, choose a toolchain and Qt version that
support that target, keep the existing feature set, and document the
compatibility adjustments. The existing bootstrap's stated target remains
64-bit Windows 10 version 1809 or newer, or Windows 11.

## Artwork, fonts, and third-party libraries

The source includes the artwork and resource files needed for the application.
The Windows source also includes its existing Noto Color Emoji font and the
font's license notices. The component licenses remain in each project's
`resources/licenses/` directory and are described in `docs/THIRD_PARTY_NOTICES.md`.
Those component licenses apply independently of the Tux-sstv project license.

## Validation

A successful build is only part of checking a compatibility port. Run the
included tests and check audio-device selection, repeated SSTV reception,
transmission cancellation, CAT/PTT, and the remote functions with your intended
hardware. Report your OS, architecture, toolchain, dependency versions, and
compatibility changes when contacting Eric about a port.
