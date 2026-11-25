# Tux-sstv for Windows and Linux

Send and receive slow-scan television pictures over amateur radio with Tux-sstv, developed by **Eric Richards, KI5BZO**.

## Downloads

| Platform | Version | Download |
| --- | --- | --- |
| Windows 10 (1809+) / Windows 11, 64-bit | 1.15.3 | [Download Windows installer](https://github.com/ericrichards420-art/Tux-sstv/releases/download/v1.15.3/Tux-sstv-1.15.3-windows-x64-setup.exe) |
| Ubuntu 26.04, 64-bit Intel/AMD | 1.14.3 | [Download Linux .deb](https://github.com/ericrichards420-art/Tux-sstv/releases/download/linux-v1.14.3/tux-sstv_1.14.3-1.ubuntu26.04_amd64.deb) |

### Windows installation

1. Run the downloaded installer.
2. Open **Tux-sstv** from the Windows Start menu. The installer also offers a desktop shortcut.
3. Use the **Setup wizard** to select your audio interface and configure radio control or a remote connection.

Requires **64-bit Windows 10, version 1809 or newer, or Windows 11**. The installer includes the application and its runtime dependencies; you do not need a compiler or a separate Qt installation.

### Linux installation

The Linux `.deb` is built for **Ubuntu 26.04 on 64-bit Intel/AMD PCs (amd64)**. It requires the runtime library and emoji-font versions available for that Ubuntu release; it is not a universal package for every Linux distribution.

After downloading the package, open a terminal in its download folder and run:

```bash
sudo apt install ./tux-sstv_1.14.3-1.ubuntu26.04_amd64.deb
```

APT installs the required dependencies. Open **Tux-sstv** from the application menu, or run `tux-sstv`, then use the **Setup wizard** to choose audio, CAT, and remote settings.

## Features

- 43 SSTV modes, with automatic reception and mode detection.
- Live waterfall and received-image display.
- Image preparation with text, emoji, templates, and Tux artwork.
- SignaLink USB, radio USB sound cards, and other audio interfaces.
- Hamlib CAT/PTT control, including a separate CAT cable alongside your audio interface.
- Remote shack operation over Wi-Fi, with radio audio and microphone support.
- Radio profiles for saving different station configurations.

## Latest updates: Windows 1.15.3 / Linux 1.14.3

- Improves emoji picker sizing so complete emoji fit inside their cells.
- Preserves spaces in text placed on SSTV images.
- Includes the setup option for an audio interface together with a separate CAT cable.

## SignaLink with a separate CAT cable

Select **SignaLink USB** in audio setup and choose its sound device, often named **USB AUDIO CODEC**. In CAT setup, choose **Use audio interface + separate CAT cable**, then select your radio model, COM port, and baud rate. This lets the audio interface handle audio-operated PTT while CAT controls frequency and mode.

## Support

Use the [Issues page](https://github.com/ericrichards420-art/Tux-sstv/issues) to report a problem. Include your Tux-sstv version, operating system and version, radio model, audio interface, and what happened.

Created and developed by **Eric Richards, KI5BZO**.
