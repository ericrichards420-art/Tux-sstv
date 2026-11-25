# Third-party software and data notices

Tux-SSTV uses the libraries, tools, and data described below. These notices
identify their licenses and sources. Eric Richards's original application code
and project material are covered separately by [the project license](../LICENSE.md).
These component notices do not replace that license or the applicable third-party
terms, and are not a legal clearance of the complete application.

## Distribution arrangement

The project's Debian package installs Tux-SSTV, its resources, and its
documentation. Its build and packaging rules use system Qt and Hamlib libraries
and declare external distribution packages for audio helpers, Qt plugins, and
the emoji font. These third-party library, helper, and font binaries are not
bundled by the current install rules.

Windows-port development kit exception: the isolated 1.15.0 port embeds an
unmodified Noto Color Emoji 2.051 font for consistent flags and joined emoji.
Its SIL Open Font License and copyright notices are distributed in
`resources/licenses/NotoColorEmoji-OFL.txt` and
`resources/licenses/NotoColorEmoji-Debian-copyright.txt`, and installed into
the Windows bundle's `licenses` directory. Font source/version/hash details
are in `resources/fonts/README.txt`. This also applies to Linux host builds
of this development copy; the separate Linux 1.14.0 release is unchanged.

The actual component versions depend on the distribution and its installed
updates. The copyright files and source-package metadata for those exact
installed packages provide the full component notices; instructions for finding
them appear below. This document does not assert that every file within a Qt,
Hamlib, or other dependency package has the same license.

## Qt 6

Tux-SSTV uses Qt 6 Core, Gui, Widgets, Concurrent, and Network. It also uses the
distribution's Qt platform and SVG plugins, with Wayland support recommended
where appropriate. Qt Test is used for development tests.

The LGPLv3 option for the relevant open-source Qt libraries covers those
libraries and their use by Tux-SSTV. The accompanying texts are:

- `resources/licenses/LGPL-3.0.txt` in the source tree, installed as
  `share/doc/tux-sstv/licenses/LGPL-3.0.txt`.
- `resources/licenses/GPL-3.0.txt` in the source tree, installed as
  `share/doc/tux-sstv/licenses/GPL-3.0.txt`. LGPLv3 incorporates GPLv3 with
  additional permissions; including this text does not declare Tux-SSTV itself
  GPL-licensed.

Qt has multiple licensing options, file-specific copyright holders, and
additional third-party components. Preserve the full notices supplied with the
exact distribution packages. Consult [Qt licensing](https://doc.qt.io/qt-6/licensing.html),
[third-party code in Qt](https://doc.qt.io/qt-6/licenses-used-in-qt.html), and the
installed packages' copyright files for those details.

## Hamlib

Tux-SSTV uses the public Hamlib 4 library API. Hamlib's frontend and backend
library code is licensed under the GNU Lesser General Public License version
2.1 or, at the recipient's option, a later version. That license covers Hamlib
and its use here. Its text is included at `resources/licenses/LGPL-2.1.txt` and
installed as `share/doc/tux-sstv/licenses/LGPL-2.1.txt`.

Hamlib's notices credit the Hamlib Group and the contributors listed in the
upstream and distribution copyright records. See the upstream
[LICENSE](https://github.com/Hamlib/Hamlib/blob/master/LICENSE) and
[AUTHORS](https://github.com/Hamlib/Hamlib/blob/master/AUTHORS), together with the
copyright file for the installed Hamlib package.

The separately installed `rigctl` utility supplies the radio model catalog.
Hamlib utilities and examples use GPLv2-or-later terms as described in the
upstream license notice. They are supplied by the distribution's
`libhamlib-utils` package, rather than copied into this application's package.

## Use of modified LGPL libraries

The intended distribution arrangement dynamically links to compatible system
libraries. Users may replace the LGPL libraries with interface-compatible
modified versions. Nothing in these notices restricts the modification or
reverse engineering permitted by the applicable LGPL terms, including debugging
library modifications.

Release maintainers must preserve a suitable library replacement mechanism and
ensure application terms respect those rights. A static build or a package
that bundles library binaries requires a separate review of relinking,
corresponding-source, installation-information, and notice requirements.

## Unicode emoji data and font

The embedded emoji catalog contains Unicode 17.0 data and Unicode CLDR English
annotations. Its provenance is recorded in `resources/emoji_catalog.json` and
`docs/SOURCES.md`. Copyright © 1991–2026 Unicode, Inc. The data is distributed
under [Unicode License v3](https://www.unicode.org/license.txt); the retained
copyright and permission notice is at `resources/licenses/UNICODE.txt`,
installed as `share/doc/tux-sstv/licenses/UNICODE.txt`.

The maintainer's catalog generator obtains data through installed ICU and IBus
interfaces. The project documents this data extraction separately from those
projects' implementation code.

Emoji glyphs are rendered using the distribution's Noto Color Emoji font. The
font is an external `fonts-noto-color-emoji` dependency, not an embedded font in
the Tux-SSTV package. Consult that package's copyright file and the
[Noto Emoji project](https://github.com/googlefonts/noto-emoji) for its SIL Open
Font License and full attribution. The 1.15.0 Windows-port development copy
instead embeds that font as described above; users do not install it globally.

## Other external runtime programs

The application uses distribution packages `alsa-utils`, `gstreamer1.0-tools`,
`gstreamer1.0-plugins-base`, and `gstreamer1.0-plugins-good`. Their programs and
plugins have their own copyright holders and licenses. Consult each installed
package's copyright file for its complete notices. This application does not
replace those notices or license those external programs on their authors'
behalf.

## Exact installed notices and corresponding distribution sources

On Debian and Ubuntu, a binary package's copyright information is normally at
`/usr/share/doc/PACKAGE/copyright`. Use the installed package name, including
any versioned library suffix; some documentation directories are symlinks.
For example, Qt library packages may use names such as `libqt6core6t64`, while
the platform plugin package is `qt6-qpa-plugins`.

Record the installed binary and source versions with:

```sh
dpkg-query -W -f='${binary:Package}\t${Version}\t${source:Package}\t${source:Version}\n' \
  'libqt6*' 'qt6-*' 'libhamlib*' \
  alsa-utils fonts-noto-color-emoji \
  gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good
```

Only installed packages are relevant; unmatched package patterns may produce
a diagnostic. To locate the copyright file for one installed package:

```sh
dpkg-query -L PACKAGE | grep '/copyright$'
```

The main source-package locations are listed below. Select the source version
reported by `dpkg-query`, rather than assuming the newest upstream release
matches the installed binary. Each distribution source package contains its
`debian/copyright` file with the corresponding detailed notices.

| Components | Source package | Ubuntu sources | Debian sources |
| --- | --- | --- | --- |
| Qt Core, Gui, Widgets, Concurrent, Network, Test, platform plugins | `qt6-base` | [qt6-base](https://launchpad.net/ubuntu/+source/qt6-base) | [qt6-base](https://sources.debian.org/src/qt6-base/) |
| Qt SVG plugin | `qt6-svg` | [qt6-svg](https://launchpad.net/ubuntu/+source/qt6-svg) | [qt6-svg](https://sources.debian.org/src/qt6-svg/) |
| Qt Wayland support | `qt6-wayland` | [qt6-wayland](https://launchpad.net/ubuntu/+source/qt6-wayland) | [qt6-wayland](https://sources.debian.org/src/qt6-wayland/) |
| Hamlib library and utilities | `hamlib` | [hamlib](https://launchpad.net/ubuntu/+source/hamlib) | [hamlib](https://sources.debian.org/src/hamlib/) |
| ALSA command-line tools | `alsa-utils` | [alsa-utils](https://launchpad.net/ubuntu/+source/alsa-utils) | [alsa-utils](https://sources.debian.org/src/alsa-utils/) |
| GStreamer tools | `gstreamer1.0` | [gstreamer1.0](https://launchpad.net/ubuntu/+source/gstreamer1.0) | [gstreamer1.0](https://sources.debian.org/src/gstreamer1.0/) |
| GStreamer base plugins | `gst-plugins-base1.0` | [gst-plugins-base1.0](https://launchpad.net/ubuntu/+source/gst-plugins-base1.0) | [gst-plugins-base1.0](https://sources.debian.org/src/gst-plugins-base1.0/) |
| GStreamer good plugins | `gst-plugins-good1.0` | [gst-plugins-good1.0](https://launchpad.net/ubuntu/+source/gst-plugins-good1.0) | [gst-plugins-good1.0](https://sources.debian.org/src/gst-plugins-good1.0/) |
| Noto Color Emoji font | `fonts-noto-color-emoji` | [fonts-noto-color-emoji](https://launchpad.net/ubuntu/+source/fonts-noto-color-emoji) | [fonts-noto-color-emoji](https://sources.debian.org/src/fonts-noto-color-emoji/) |

With the matching distribution source repositories enabled, the corresponding
source can also be obtained using `apt-get source SOURCE_PACKAGE=SOURCE_VERSION`,
replacing those placeholders with the recorded source package and version.
Source repositories must match the distribution that supplied the binary.

These locations describe the current external-package arrangement. If a future
release redistributes third-party binaries, its maintainer must fulfill that
release's applicable source-distribution obligations for the exact shipped
versions; a generic upstream URL alone should not be treated as a source offer.
