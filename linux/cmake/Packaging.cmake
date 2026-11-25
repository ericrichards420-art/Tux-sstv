# Build a native binary package for the distribution used to compile it.
# dpkg-shlibdeps resolves the installed Qt/C++ runtime package names and versions.
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "tux-sstv")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Tux-sstv")
set(CPACK_PACKAGE_CONTACT "Tux-sstv")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Send and receive SSTV pictures over amateur radio")
set(CPACK_PACKAGE_DESCRIPTION "Tux-sstv ${PROJECT_VERSION} Final Release brings slow-scan television to your Linux desktop.
Fit complete emoji shapes in the picker and preserve word spacing in image
captions and template fields with regular system text font fallbacks.
Share pictures over amateur radio with 43 SSTV modes, automatic receive mode
detection, a live waterfall and a built-in picture editor.
Read live Morse callsigns automatically in RX, with decoded text and estimated
tone/speed on local or remote computers. Confirm a decoded callsign before
using it for Reply, contact logging or received image metadata.
Automatically recover a local or shack audio capture process that stops
delivering audio data, without restarting during ordinary radio silence.
Receive mode headers tolerate brief audio dropouts while retaining parity,
checksum and preamble validation.
Automatic reception can also acquire a complete mode code when part of
the opening tone is clipped or the receive input opens late.
Take selfies or choose screenshots while receiving images and listening
to shack audio without interruption.
Choose from 14 included full-size Tux radio cartoons and use them in any SSTV mode.
Celebrate on the air with Tux pictures for all 11 U.S. federal holidays,
plus Easter and Halloween, in a separate Holidays gallery.
This update improves PTT cancellation and recovery, remote voice startup,
and stalled SignaLink or manual playback handling.
.
The Setup wizard guides audio devices and levels, CAT/Hamlib radio control,
and local or remote station connections. Choose and adjust a SignaLink USB,
a radio's built-in USB sound card or another audio interface. SignaLink USB
works with any compatible radio, independently of CAT model selection.
Use a separate CAT cable at the same time: keep audio on the sound interface
and choose a CAT serial port and baud rate for frequency and mode control.
Its USB Audio CODEC choices are clearly labeled and select receive and transmit
together while preserving saved device settings. Prepare pictures with clean
text, callsigns, templates and the complete Unicode 17.0 emoji collection, select a saved screenshot or take a webcam selfie.
Create weather cards with temperature, wind, rainfall, humidity and barometric pressure.
Build TV-style news bulletins, scoreboards and program cards in the TV Studio gallery.
.
The Radio tab combines a large frequency display, segmented signal meter,
filter/power/RIT readouts, tuning controls and click-to-lock voice PTT in one
panel. Frequency and PTT stay visible while scrolling to waterfall and audio levels.
Hamlib provides radio and PTT controls. Named radio profiles remember each radio's
audio devices, levels and CAT/PTT setup, including remote shack profile selection. Save received pictures, browse receive
history, and encode or decode WAV recordings. Local SSTV operation needs no
account. Connect a laptop to your shack PC over home Wi-Fi to use its radio and
USB audio remotely. Click PTT in the Radio tab to send your laptop microphone
through the shack radio, then click again to stop. Listen to the receiver on
laptop speakers or a headset, with independent volume and automatic muting
during transmission. A microphone check provides live levels, clipping warnings
and adjustable voice gain without keying the radio. Setup also works from
the laptop to configure the shack sound interface and CAT connection. A suitable radio and audio connection are needed for
on-air operation.")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_STRIP_FILES TRUE)
set(CPACK_DEBIAN_PACKAGE_NAME "tux-sstv")
set(CPACK_DEBIAN_PACKAGE_PROVIDES "opensstv")
set(CPACK_DEBIAN_PACKAGE_REPLACES "opensstv")
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "opensstv")
set(CPACK_DEBIAN_PACKAGE_SECTION "hamradio")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# Audio helpers and Qt's dynamically loaded platform plugins do not appear in
# ELF NEEDED entries, so they must be declared in addition to shlibdeps output.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "alsa-utils, libhamlib-utils, qt6-qpa-plugins, qt6-svg-plugins, fonts-noto-color-emoji (>= 2.051), gstreamer1.0-tools, gstreamer1.0-plugins-base, gstreamer1.0-plugins-good")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-wayland")
set(OPENSSTV_DEBIAN_RELEASE "1" CACHE STRING "Debian package revision")
if(EXISTS "/etc/os-release")
  file(STRINGS "/etc/os-release" _opensstv_os_id REGEX "^ID=")
  file(STRINGS "/etc/os-release" _opensstv_os_version REGEX "^VERSION_ID=")
  string(REGEX REPLACE "^ID=\"?([^\"]*)\"?$" "\\1" _opensstv_os_id "${_opensstv_os_id}")
  string(REGEX REPLACE "^VERSION_ID=\"?([^\"]*)\"?$" "\\1" _opensstv_os_version "${_opensstv_os_version}")
  set(CPACK_DEBIAN_PACKAGE_RELEASE "${OPENSSTV_DEBIAN_RELEASE}~${_opensstv_os_id}${_opensstv_os_version}")
else()
  set(CPACK_DEBIAN_PACKAGE_RELEASE "${OPENSSTV_DEBIAN_RELEASE}")
endif()
include(CPack)
