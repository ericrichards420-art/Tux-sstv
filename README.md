# Tux-sstv for Windows

Send and receive slow-scan television pictures over amateur radio with Tux-sstv, developed by **Eric Richards, KI5BZO**.

## Download and install

Open the [Releases page](https://github.com/ericrichards420-art/Tux-sstv/releases) and download **Tux-sstv-1.15.3-windows-x64-setup.exe** from the release assets.

1. Run the downloaded installer.
2. Open **Tux-sstv** from the Windows Start menu. The installer also offers a desktop shortcut.
3. Use the **Setup wizard** to select your audio interface and configure radio control or a remote connection.

Requires **64-bit Windows 10, version 1809 or newer, or Windows 11**. The installer includes the application and its runtime dependencies; you do not need a compiler or a separate Qt installation.

## Features

- 43 SSTV modes, with automatic reception and mode detection.
- Live waterfall and received-image display.
- Image preparation with text, emoji, templates, and Tux artwork.
- SignaLink USB, radio USB sound cards, and other audio interfaces.
- Hamlib CAT/PTT control, including a separate CAT cable alongside your audio interface.
- Remote shack operation over Wi-Fi, with radio audio and microphone support.
- Radio profiles for saving different station configurations.

## Version 1.15.3

- Improves emoji picker sizing so complete emoji fit inside their cells.
- Preserves spaces in text placed on SSTV images.
- Includes the setup option for an audio interface together with a separate CAT cable.

## SignaLink with a separate CAT cable

Select **SignaLink USB** in audio setup and choose its sound device, often named **USB AUDIO CODEC**. In CAT setup, choose **Use audio interface + separate CAT cable**, then select your radio model, COM port, and baud rate. This lets the audio interface handle audio-operated PTT while CAT controls frequency and mode.

## Support

Use the [Issues page](https://github.com/ericrichards420-art/Tux-sstv/issues) to report a problem. Include your Tux-sstv version, Windows version, radio model, audio interface, and what happened.

Created and developed by **Eric Richards, KI5BZO**.
