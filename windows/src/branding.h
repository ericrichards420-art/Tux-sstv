#pragma once
#include <QPixmap>
#include <QIcon>
namespace branding {
inline constexpr char Name[]="Tux-sstv";
inline constexpr char ReleaseLabel[]="Windows port development preview";
// Keep Qt's established storage and desktop identities across the rename.
inline constexpr char StorageName[]="OpenSSTV";
inline constexpr char DesktopId[]="opensstv";
QPixmap tux(int size);
QIcon icon();
}
