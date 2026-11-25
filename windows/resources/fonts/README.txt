Noto Color Emoji 2.051 - embedded application font

This is an unmodified copy of NotoColorEmoji.ttf from Ubuntu's
fonts-noto-color-emoji 2.051-1build1 package. It uses CBDT/CBLC bitmap color
glyphs and covers the Unicode 17 emoji catalog used by this application.

SHA-256:
9fd0a3d0ce84d77e3185dfbae77bd1abf3926aa49a032e354d076c4f17151f10

Source project: https://github.com/googlefonts/noto-emoji
Release source: https://github.com/googlefonts/noto-emoji/tree/v2.051
Font metadata source commit: e92753bfa55fd449e427d4d325f9c8c40408c74e
Distribution source: https://launchpad.net/ubuntu/+source/fonts-noto-color-emoji

The font is distributed under the SIL Open Font License 1.1. Full font
copyright/license notices are included in ../licenses/NotoColorEmoji-OFL.txt
and ../licenses/NotoColorEmoji-Debian-copyright.txt. The font's embedded
metadata also retains its original copyright/license information.

emoji_font.qrc embeds the complete font in applications/tests that use the
shared SSTV core. ui::emojiFontFamilies registers it for the current process
through QFontDatabase. It does not install a system font or require admin
rights. Text captions, the editor and the picker use the same font family.

Windows builds require Qt 6.9-6.12, which supports bitmap color fonts:
https://www.qt.io/blog/emoji-in-qt-6.9
