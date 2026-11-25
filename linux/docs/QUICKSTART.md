# Tux-sstv quickstart

This guide covers local SSTV and operation from another computer on your home
network, including built-in picture galleries and the PTT reliability fixes.

Choose **Holidays** below the TX/RX image area for all 11 U.S. federal holidays,
plus Easter and Halloween. Click one,
add your callsign or greeting, and select any SSTV mode. **Tux pictures** keeps
the original 14-image radio-adventure collection.

## 1. Choose the right installer

The 1.14.3 installer targets **Ubuntu 26.04, amd64**: ordinary
64-bit Intel or AMD PCs. It requires the Qt and emoji-font versions available
for that Ubuntu release. Build separate packages for other platforms.

For the 1.14.3 package built on Ubuntu 26.04 amd64, open a terminal in the
folder containing the package and run:

```bash
sudo apt install ./tux-sstv_1.14.3-1~ubuntu26.04_amd64.deb
```

APT installs the required runtime packages. Start **Tux-sstv** from the
application menu, or run `tux-sstv` in a terminal. Use the same application
version on the shack PC and remote laptop.

If APT reports unavailable or incompatible dependencies, check the Ubuntu
release and CPU architecture. Use a build made for that system; do not force
installation or bypass its dependency checks. See the README's build section
to create a native package for another target, then check it on that system.

## 2. Set up the shack PC first

Connect the radio's audio interface and, if used, its CAT interface to the
shack PC. CAT provides radio control; the application also needs a receive and
transmit audio path, such as a SignaLink USB or the radio's built-in USB audio.

Open **Option → Setup wizard…**:

1. Choose local operation initially.
2. Open the audio check. For SignaLink, choose **Audio interface → SignaLink
   USB**, then select its **SignaLink USB — USB Audio CODEC** sound device.
   This sets receive and transmit together for any compatible radio. For
   another interface, select its sound device instead. Check the input meter
   and choose a sensible transmit level. Finish the audio check to save.
3. For an audio interface plus a CAT cable, click **Use audio interface +
   separate CAT cable** in the CAT step, choose the radio model and cable port,
   and match the CAT baud rate to the radio. The interface handles PTT while
   CAT controls frequency and mode. [Full steps](SEPARATE_CAT.md). Leave
   Hamlib disabled if you only want audio and have no CAT connection.
4. Review and save the setup. Choose **Radio → Connect** when ready to use CAT;
   the wizard does not connect or key the radio automatically.

Audio test tones are explicit actions. A SignaLink or a radio using VOX can key
when a tone plays, so use your station's normal test arrangement.

For the TS-990S with USB audio, the source documentation describes **USB-D1**
and **Use USB/data PTT**. The radio's DATA SEND source must enable USB Audio,
and the selected output must be its USB sound device. Follow
[RADIO.md](RADIO.md) for the full radio settings and backend limitations.

## 3. Receive and send a picture locally

Reception starts on the selected input. Watch the waterfall and input level. Version
Automatic local/shack reception retries if its audio capture process stays
open but delivers no audio data for eight seconds; ordinary quiet audio does
not cause restarts. Keep 1.14.0 on both computers for remote operation.

1.14.0 can acquire a valid mode code after a clipped opening tone. The mode code
still needs to arrive intact. Automatic reception and detection start on their
own; the Record audio control is for saving a WAV recording.
Use automatic receive mode detection or select a known mode as appropriate.

For a transmit picture, open **Tux pictures** below the image area and select
one of the 14 included cartoons, or choose your own image or a test pattern.
The Tux originals are full-size and can be used in any mode. Add a
callsign/text if desired, and choose the SSTV mode. **Save TX WAV…** exports audio
without keying the radio. **TX audio** uses the configured audio/PTT path.
Click **Stop**, or **Stop / PTT OFF** on the Radio tab, to stop transmission.

The application implements 43 modes. The source compatibility table separately
tracks independent interoperability evidence; selectable modes do not by
themselves establish compatibility with every other program or radio setup.

## 4. Connect the living-room laptop

Once the shack works locally:

| Computer | Setting |
| --- | --- |
| Shack PC | Open **Remote → Remote station…**, choose **Share this shack PC**, and save. Reopen it to read the address, port and connection key. |
| Laptop | Open the same dialog, choose **Connect to shack PC**, enter those three values, and save. |

Keep the shack PC awake with Tux-sstv running. The default port is **48765**.
Only one laptop controls a shared shack at a time. The picture editor and
radio controls on the laptop use the shack's audio and radio connection.

This feature is designed for a trusted home network. Keep the connection key
private. Pairing authenticates the connection; the current protocol does not
encrypt its audio and control traffic. See [REMOTE.md](REMOTE.md) for details.

## 5. Listen and talk from the laptop

In the laptop's **Radio** tab:

1. Enable **Listen to shack** and choose **Laptop speakers**. `default` follows
   the computer's default audio output; select your Bluetooth speaker in the
   desktop audio settings if that is the output you want.
2. Select **This computer's microphone**. Use **Find audio devices** if needed.
3. Open **Mic check…**, speak normally and adjust mic gain. This checks the
   laptop microphone without transmitting or saving a recording.
4. Click **VOICE PTT / Click to transmit**, wait for **Talk now**, then speak.
   Click again to return to receive. Listening mutes during transmission.

Mic gain, laptop speaker volume and the shack's transmit output level are
separate controls. Software gain cannot repair audio already clipped at the
microphone. A SignaLink uses speech/audio to key; Hamlib PTT behavior depends on
the selected method and radio backend.

If PTT OFF cannot be confirmed, check the radio directly and restore its receive
state before continuing. A lost CAT connection is not proof that a physical
transmitter has unkeyed.

## 6. Useful troubleshooting details

For a support request, record the Tux-sstv version on both computers, Ubuntu
version, CPU architecture, radio model, interface, selected audio devices and
exact error message. Describe whether the problem happens locally, remotely,
or both. Do not include the remote connection key.

The full README, [RADIO.md](RADIO.md), [REMOTE.md](REMOTE.md) and
[COMPATIBILITY.md](COMPATIBILITY.md) cover advanced setup. Dependency license
information is in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Read a Morse callsign

Keep receiving normally on either the shack PC or remote laptop. In RX, the
Morse row automatically follows one CW tone and shows the decoded text plus
estimated tone and speed. You can select and copy that text. Check a complete
callsign, then click Use callsign to use it for Reply and Log contact. A saved
RX picture records the accepted ID as Morse (confirmed). Existing FSKID stays
unchanged until you explicitly confirm a different Morse callsign.

Best results use a clear single tone around 400–2500 Hz, 5–40 WPM with regular
character/word spacing. Noise and multiple stations can produce wrong text;
this is a reading aid. It runs from live receive audio without starting a
second capture or keying the radio. SSTV reception and following images keep
working. The live reader does not add Morse decoding to Open RX WAV.
