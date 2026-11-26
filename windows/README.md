# Tux-sstv Windows port 1.15.3

**Source terms:** Personal Linux/Windows compatibility compilation only;
redistribution requires separate written permission. See [LICENSE.md](LICENSE.md).
Original developer: **Eric Richards, KI5BZO**. Improvement proposals and
permission requests: **ericrichards420@gmail.com**.

Developed by **Eric Richards — KI5BZO**.

Version 1.15.3 keeps full emoji shapes inside their picker cells at scaled
display sizes. Image captions and template fields use a normal system text
font for letters and spaces; emoji retain their color font. Repeated spaces
and line breaks stay intact in the caption editor and image composition.


This Windows source kit supports SignaLink or another sound interface together
with a separate CAT cable. Select the audio device in audio setup, then use
**Use audio interface + separate CAT cable** in CAT setup. Choose the model,
CAT COM port and matching baud rate. Frequency/mode control stays available
while the audio interface handles PTT. [Setup guide](docs/SEPARATE_CAT.md).

Extract the whole kit on Windows and run **Install and Build Tux-sstv.cmd**
to build version 1.15.3, run native tests and create the Setup.exe installer
with its Start menu icon. See [WINDOWS-START-HERE.txt](WINDOWS-START-HERE.txt).
The remaining sections also document the shared application and Linux builds.


This directory is an isolated Windows port of the tested Linux 1.14.0 release.
Read **WINDOWS-START-HERE.txt** for native Windows build and test instructions.
The target is 64-bit Windows 10 (1809+) or Windows 11, using Qt 6.9-6.12.
Codex is not needed on the Windows build computer.
Extract the kit and double-click **Install and Build Tux-sstv.cmd** to download
MSYS2, install dependencies, compile, test and package the preview automatically.
The kit contains source and build scripts, not a verified Windows executable.
Windows-specific code must be compiled and exercised on Windows before release.

The port adds a native Windows PCM helper, band-limited format conversion,
stable audio endpoint selection, COM-port discovery, a video-only Qt camera
path, Windows dependency deployment and optional per-user installer packaging.
It retains the SSTV/Morse engines, picture collections, profiles and Wi-Fi
protocol. Windows hardware sound-device levels remain manual in this preview.
The port embeds Noto Color Emoji 2.051, including its license, so flag and
joined-emoji rendering works without installing an extra Windows font.
The original installed Linux release and its backups remain separate.

The following documentation describes the original Linux release and shared
features; its Ubuntu install commands apply to Linux. Use the Windows text
guide for this development port.

---

# Tux-sstv 1.14.0 — Final Release

Developed by **Eric Richards — KI5BZO**.

New in **1.14.0**: automatically read Morse callsigns from the live receive
audio on the shack PC or remote laptop. The RX tab shows decoded text and
estimated tone/speed; select text to copy it, or click **Use callsign** after
checking it to fill Reply and Log contact and label the saved image. Morse
stays separate from checksum-validated FSKID and never silently replaces it.
The reader follows one clear CW tone, approximately 400–2500 Hz at 5–40 WPM.
Noise, overlapping stations and unusual spacing can cause errors. It shares
the existing capture and receive worker; no extra audio device or PTT is used.
SSTV picture audio is excluded, and CW after an image is preserved across
block boundaries. This reader is for live reception; Open RX WAV continues
to decode SSTV images and FSKID.

Automatic reception can start when part of the opening tone
is clipped or the receive input opens late, provided the full valid mode code
is received. Coverage includes all 43 modes, both local and remote input,
and consecutive transmissions. Mode-word checks remain required; an active
image is replaced only after its stricter full-preamble check succeeds.
Automatic tuning measures the remaining leader after late acquisition.
Automatic local/shack reception also retries an audio capture process that
stays alive but delivers no PCM for eight seconds. Quiet PCM is healthy and
does not trigger a restart. Shack recovery restores its stream to the laptop.

Automatic reception also tolerates brief audio dropouts in the
mode header. It samples several positions within each symbol while still
requiring valid parity/checksums and the expected preamble. Regression tests
cover standard, Scottie, extended and narrow headers and consecutive live
images with damaged headers.

Taking a selfie or choosing a screenshot also keeps automatic reception and
remote laptop listening active throughout the dialog, including capture,
Use photo and Cancel. Preparing a picture no longer resets an incoming frame.

**Holidays** has Tux radio pictures for **all 11 annual
U.S. federal holidays**, plus Easter and Halloween. Select a full-size picture,
add your callsign or greeting, then choose any SSTV mode. The original
14-image Tux gallery and the 1.13.1 PTT reliability fixes are included.

The federal set covers New Year's Day, Martin Luther King Jr. Day,
Washington's Birthday, Memorial Day, Juneteenth, Independence Day, Labor Day,
Columbus Day, Veterans Day, Thanksgiving Day and Christmas Day.
See [artwork notes](docs/ARTWORK.md) for the checked OPM source and provenance.

**Send and receive SSTV pictures over amateur radio from your Ubuntu desktop.**
Tux-sstv includes 43 SSTV modes, automatic reception and mode detection, a live
waterfall, guided audio setup, Hamlib radio controls and a built-in picture editor.
Add text and callsigns, choose saved screenshots or take a webcam selfie, then
share your picture. Local SSTV operation needs no account.

Version **1.14.0** includes **14 full-size Tux radio cartoons** in the
**Tux pictures** gallery. Select a picture, add your callsign or text, and
choose any SSTV mode. The original 1402 × 1122 artwork stays intact as you
switch modes; it is fitted to the selected mode when composing the TX image.
No separate image download is needed. See [artwork notes](docs/ARTWORK.md).

This release also fixes PTT recovery and cancellation, guards remote voice
startup after a lost connection, and stops stalled manual/SignaLink playback
after its expected duration. Use **1.14.0 on both computers** for the remote
PTT-test ownership protection. The package includes a quickstart and
third-party dependency notices.

A suitable radio and audio connection are needed for on-air operation. Start
with the [quickstart](docs/QUICKSTART.md). Mode implementation and independent
interoperability evidence are tracked in [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

Tux-sstv uses Qt 6 under LGPLv3 and Hamlib under LGPLv2.1-or-later. See
[third-party notices](docs/THIRD_PARTY_NOTICES.md) and the accompanying license
texts. These component licenses apply independently of the application terms in
[LICENSE.md](LICENSE.md). The original application source is supplied for personal
Linux/Windows compatibility compilation; redistribution requires separate written
permission from Eric Richards, KI5BZO.

Ubuntu App Center can still show its standard third-party package warning when
opening a local `.deb`. Application descriptions and release labels do not change
the package's source or grant Ubuntu verification.

## Guided station setup — 1.16.0

Choose a connection preset for radio USB/CAT, SignaLink with CAT, FlexRadio SmartSDR CAT, native SmartSDR networking, or rigctld. Audio devices and transmit keying have separate choices. Flex DAX exposes separate RX and TX selectors with readable device names. The final setup page reports the actual keying configuration, offers a connection check, and has an optional one-second PTT test that releases when the wizard closes or leaves the review page. Existing radio profiles keep their saved settings. See [the setup guide](docs/SETUP_GUIDE.md).

## A new name, the same station

**OpenSSTV is now Tux-sstv.** Eric Richards — KI5BZO remains credited as its
developer. The Tux penguin appears in the desktop icon, About window, radio
header and audio wizard. The mascots are interface decorations; they do not
alter transmitted pictures.

The application and package command is **`tux-sstv`**. The former `opensstv`
command and desktop ID remain compatible, so existing shortcuts and dock pins
continue to launch it. Installing the renamed package replaces the old package.
Your radio/audio settings, remote key, history, layouts, recordings and contacts
use the existing storage locations; no files are moved or discarded. Existing
`Pictures/OpenSSTV` folders retain their names intentionally.

Use **Tux-sstv 1.14.0** on both computers. Older OpenSSTV
releases remain protocol-compatible with the remote features they support;
minimum versions 1.2–1.3 mentioned below refer to those pre-rename releases.

## Open setup and choose the correct computer

**Option → Settings…**, **Setup wizard…** and **Radio setup…** stay
reachable. If transmission or locked voice PTT is blocking setup, choose
**Stop and open setup**; configuration waits for PTT release. Cancel leaves the
transmission running.

Settings shows **Local USB**, **Shared shack PC** or **Remote laptop**. If a radio
such as the **Kenwood TS-990S** is connected to this computer but Tux-sstv is still
in remote mode, choose **Set up this computer's USB radio/audio…**. This switches
to local operation while retaining the saved remote address and key. The radio
and audio setup dialogs offer the same local path when the shack is unavailable.

For Hamlib, enable radio control, search for **TS-990S**, choose the CAT device
connected to the target computer, save and use **Radio → Connect**. If the radio
is on the shack PC, keep remote mode and open its Hamlib setup instead. No radio
connection or PTT starts automatically from these setup shortcuts.

## Named radio profiles

The Radio tab now has a **radio profile selector** and **Profiles…**. Your existing
setup becomes **Current radio** automatically. Rename it for your current radio,
then choose **Save as new…** to make a copy for another radio. **Setup active
radio…** opens the Setup wizard to choose that radio's audio and CAT devices.

Each profile keeps CAT/Hamlib model and connection, PTT method/input/timing,
RX/TX audio devices, wizard interface type, TX level, RX gain/automatic clipping
control and receive calibration. Changes save automatically to the active profile.
Switching saves the outgoing setup, closes its CAT connection, restores the next
profile and resumes automatic reception. Use **Radio → Connect** when ready;
profile changes never connect or transmit automatically.

While remotely paired, the selector and manager operate on profiles stored on
the shack PC. The laptop uses the selected shack profile's SSTV TX level. Laptop
microphone, speakers, receive-monitor adjustments, station identity, pictures
and Wi-Fi pairing remain separate. Install **1.14.0 on both computers** for remote
profile management. See [radio profiles](docs/RADIO.md#named-radio-profiles-1130).

## Full emoji collection

**Text & emoji…** includes the complete **Unicode Emoji 17.0** collection:
3,953 emoji and components, with searchable names and keywords, ten categories,
all country flags, people, professions, families and joined sequences. Use a
preferred skin tone for ordinary browsing, or choose **All skin tones** to see
every combination, including mixed-tone couples and handshakes. Recent remembers
the exact emoji you inserted. Click an emoji or select it and press Enter.

The picker and transmitted image use **Noto Color Emoji** artwork. These are
standard Unicode emoji also used on phones; their appearance follows the Linux
font. The Ubuntu installer requires Noto Color Emoji 2.051 or later for complete
Emoji 17.0 rendering. No internet connection is needed to browse or insert them.

## TS-990S USB audio and remote voice

Radio mode selection now applies immediately; typed modes still use **Set**.
The TS-990S list includes **USB-D1/D2/D3** and **LSB-D1/D2/D3**, with actual
radio readback. For the usual USB computer-audio setup, choose **USB-D1**
(or **LSB-D1** for the appropriate sideband). In **Radio setup…**, choose
**Use USB/data PTT**, save, then reconnect the radio. This selects the rear
DATA SEND input so laptop speech reaches the USB audio path rather than the
front microphone input. The radio's DATA SEND source must have USB Audio enabled,
and the shack's selected audio output must be the TS-990S USB sound card.

Use **Mic check** on the laptop, then click PTT, wait for **Talk now**, and speak;
click again to return to RX. The check measures laptop input and software gain,
not RF modulation or the radio's USB input level. See [radio setup](docs/RADIO.md#ts-990s-usbdata-audio-and-voice-1110)
for the radio menu references and backend limitations. Install 1.14.0 on both
computers for these fixes.

## Operate remotely over home Wi-Fi

The receive monitor keeps its activity message above a fixed **Shack PC · address**
line. Incoming audio does not replace that text, so silence, sound and radio
updates cannot alternate the address with another message. Real recording,
transmit, waiting, error and disconnect states still update; hover an audio
error for details. Remote recording reports the shack source, not laptop audio.


The earlier 1.7.1 update stabilizes remote UI updates: audio-device controls keep their
final enabled/disabled state, radio and voice PTT styling changes only when
needed, and radio-only updates leave the TX picture alone. Unchanged remote
snapshots are ignored by the laptop and no longer repeated by an updated shack.
This keeps meters and real TX/RX changes responsive while reducing repaint work.
The laptop fixes also work with older supported shack versions.


Use **Remote → Remote station…** on both computers. Choose **Share this shack
PC** on the computer connected to the radio, then choose **Connect to shack PC**
on the laptop and enter the displayed address, port and connection key. The
laptop's normal RX, picture editor, Play TX and radio controls use the shack's
USB audio and radio. See [Remote operation](docs/REMOTE.md) for setup and
connection troubleshooting.

**Radio setup…** and **Radio → Hamlib setup…** open the Hamlib radio model,
CAT device and PTT settings. With Tux-sstv on both computers (or OpenSSTV **1.3.2 or newer**),
the laptop shows and saves the shack's setup; the shack can also edit it while
paired and idle. Saving closes the existing CAT connection. Use **Radio →
Connect** afterward. The laptop's local radio configuration remains separate.

The **Setup wizard…** is available from **Option**, the **Radio** menu and
**Radio → Audio levels**. It guides you through four steps:

1. **Station and remote connection** — choose local USB, share the shack PC,
   or connect a remote laptop with the shack address, port and key.
2. **Audio devices and levels** — open the guided audio check to select the
   target computer's RX/TX devices, measure input and optimize supported USB
   playback levels. Finish the audio check, then continue in Setup.
3. **Radio CAT / Hamlib** — choose the radio model, CAT device and PTT options
   directly in the wizard. Leave Hamlib disabled for audio-only operation.
4. **Setup summary** — review the saved devices and station role.

Next saves the station connection and CAT changes in their respective steps.
The audio check saves on Finish. Closing Setup keeps changes already saved.
On a remote laptop, audio and CAT target the **shack PC**; local device profiles
remain available when you switch back to local operation. You can skip the audio
check to keep working settings. If the shack is offline, the wizard explains how
to reconnect and lets you keep its existing setup. CAT does not connect or key
the radio automatically; use **Radio → Connect** after setup.

Either computer can press **Play TX**. The shack always plays the waveform
through its connected radio and USB sound interface. When the laptop sends,
the shack shows **Remote TX monitor**. When the shack sends, the laptop switches
to RX and shows **Shack TX monitor**. Pictures build from outgoing audio as
playback progresses. A shared transmit lock prevents overlapping transmissions.
These monitors do not confirm reception by another radio station.

For voice, open the **Radio** tab on the laptop. Choose **This computer's
microphone** (use **Find audio devices** for a headset), then click **PTT · Click to transmit**.
Wait for **Talk now** and speak. PTT stays locked on after releasing the mouse;
click it again to stop and return to receive. Voice goes
through Wi-Fi to the shack's selected audio output and radio. Both computers
need Tux-sstv (or OpenSSTV **1.3.1 or newer**) for click-on/click-off PTT. Mic gain and a limiter control the streamed
level; they cannot repair clipping at the microphone itself. Hamlib PTT holds
the radio keyed; a SignaLink keys from speech and its DLY control sets the
release delay.

To hear the receiver, check **Listen to shack** in the laptop’s **Radio tab**. Choose
**Laptop speakers** (`default` uses the default audio output), or use **Find audio
devices** to list a headset or other local output. Set the adjacent speaker volume.
Listening is optional and remembered. It mutes during PTT, SSTV transmission or
setup, then resumes with fresh receive audio. Radio Disconnect immediately stops
laptop speaker playback. If CAT is enabled, listening stays paused until the
shack radio reconnects, even while Wi-Fi and incoming audio remain active.
Audio-only stations without CAT keep their normal listening behavior. Turning listening off leaves
SSTV reception and decoding running. Microphone gain, speaker volume and the
shack's radio output are separate controls. See [remote voice and listening
setup](docs/REMOTE.md#voice-ptt).

## Check your voice microphone

Open **Mic check…** in the **Radio tab** to test this computer's selected voice microphone
without transmitting. Speak normally and watch **Original microphone input**
and **Voice level with mic gain**. Green voice peaks between −18 and −6 dBFS
leave useful headroom. The check distinguishes quiet speech, good levels, high
peaks, limiter activity and clipping already present at the input.

Adjust **Mic gain** live. If the original input clips, reduce Ubuntu's microphone
input level or move farther away; lowering software gain cannot undo that
clipping. If speech is still quiet at maximum gain, move closer or raise the
Ubuntu input level. **Use settings** saves the chosen mic and gain; **Cancel**
keeps the previous choices. Stop/Start and Reset readings let you recheck.

On a remote laptop, incoming SSTV reception continues. Laptop speaker playback
mutes during the check to keep receiver audio out of the microphone, then resumes.
In local operation, automatic capture pauses until the check closes so an
exclusive audio device can be opened, then resumes automatically. No microphone
recording is saved and no check audio is sent to the shack or radio.

## Run

```bash
cd /path/to/tux-sstv
./run.sh
```

The program starts receiving automatically on the selected audio input. Open an
image or use the built-in test pattern, choose a mode, and export a transmit WAV. Open a received WAV to
decode it. With **Callsign on image** enabled, entering a callsign places white text with a thin dark outline directly over the
image, with no background band. The preview shows
the actual fitted image and text. Original images are preserved.

Open **View → Mode compatibility** for the 43 target modes and their current status.
Only implemented modes are selectable for transmit and receive.

## Stable Station / audio panel

The Station / audio summary keeps automatic reception and Hamlib status together
instead of alternating messages as audio arrives. Its two-line space stays fixed,
and long device names cannot widen the controls. Remote RX and TX devices each
have their own summary line. Hover the summary for its full text; the selected
input and output fields retain their positions during reception and recording.

## Radio console

Select the **Radio** tab beside RX, TX and Sync for Tux-sstv's own full-width
radio frontend. The console has a large frequency readout, rotary tuning dial,
step buttons, live signal meter, and mode/filter, VFO, RF power and RIT controls.
The SSTV side panel and picture strip return when you switch back to another tab.

The 1.8 interface brings station setup, radio control and voice into one themed
front panel. The frequency display includes live filter width, RF power and RIT
readouts, alongside a segmented signal meter. Frequency, **Voice PTT** and
**Stop / PTT OFF** stay visible while the lower controls scroll. **Waterfall**
and **Audio levels** jump directly to those sections; **Back to tuning** returns
to tuning. Light, dark and decorative skins apply throughout the console.

Use **Radio setup…** to configure the Hamlib connection, then **Connect**.
The interface works with the radio selected in those settings and with rigctld;
it starts disconnected. Actual radio readback fills the display. Unsupported
readings show as unavailable, and controls follow the available readback.

**M/V**, beside the VFO selector, switches between memory and VFO operation.
It is highlighted when the radio reports **MEM**. Press it again to return to
the last VFO seen during this connection; when connecting in memory mode, it
requests the radio's generic VFO mode. The radio must support memory/VFO switching
over Hamlib. Unsupported requests appear in the status line. Switching requires
VFO readback and is locked during active SSTV audio, preparation and PTT.

Enter a frequency and press **Tune**, or use the dial and **− / +** with your
chosen step. Dial changes are combined into bounded tuning requests. **Set**
applies its own mode/filter, VFO, power or RIT control; polling keeps unfinished
edits. RF power is a percentage of the radio's range. The signal meter uses the
radio's signal-strength readback and pauses during transmission.

**Stop / PTT OFF** stays available during transmission and also stops manual
SSTV audio playback. Tuning and other radio changes lock during active SSTV
audio/preparation or PTT. **Advanced controls…** opens the existing split,
repeater, CTCSS and timed PTT-test controls. Opening the console never connects,
tunes or keys a radio automatically.

## Audio levels

Open **Radio tab → Audio levels** (the button at the top of the Radio tab) for
separate TX and RX controls. **TX level** sets the volume of the generated SSTV
signal and optional Morse ID, including exported WAV files. Set it before
sending; it is locked while preparing or playing a transmission. **RX gain**
changes incoming audio before decoding and can be adjusted while receiving.
Both settings are remembered. The initial 100% TX and 0 dB RX preserve the
previous audio behavior.

Select an input/output through **Audio devices…**. **Start input meter** checks
your receiver or computer microphone for up to 30 seconds; normal RX also feeds
the meter. **Play 1 s output check** plays a short tone at the selected TX level.
These checks do not request Hamlib PTT, although a radio using VOX can respond.

Peak and RMS meters show digital headroom in dBFS. Red warns about clipping;
the input warning still appears if the original signal clipped before software
gain. Lower that level at the microphone/interface or in Ubuntu's input controls.
The green region is a headroom guide, not an RF calibration. With speakers
feeding the radio's microphone, set speaker volume, microphone distance and the
radio's input level while checking its ALC/deviation too. Automatic RX protection
reduces excessive RX gain without compression. The Radio tab has a separate
voice microphone gain and limiter. Adjust source levels before receiving or
transmitting when possible.

## Live audio waterfall

The **Audio monitor · Hz** panel now offers **Waterfall** and **Spectrum** views.
The waterfall shows the latest 30 seconds of live input from **500–3000 Hz**,
with new audio at the top. Brightness/color indicates audio amplitude on a fixed
−90 to 0 dBFS scale. Frequency marks help identify sync tones, drift and nearby
audio interference. This displays receiver audio, not a radio RF spectrum.

Tux-sstv automatically opens the selected receive input at startup, updates the
waterfall and RX meter, and decodes incoming images. No Live audio button or
startup option is needed. Decoded pictures save automatically. The source and
receiving/paused/stopped state appear above the waterfall. **Input open · silence**
means the stream is supplying zero samples; check the selected input and routing.

**Record audio** optionally saves incoming audio to a WAV file. It uses the same
open stream and receiver, so starting a recording does not interrupt reception.
**Finish recording** saves the WAV while reception continues.
Reception pauses for transmit/PTT, Settings and the audio wizard, then resumes.
**Stop** cancels the current operation and releases PTT, then returns to automatic
reception. Auto is already selected at startup: no mode or Record button is
needed to begin listening. Temporary input failures display an error and retry
automatically, starting after one second and backing off to ten seconds.
When the device becomes available, reception resumes on its own.

**Automatically prevent RX clipping** is enabled by default in Radio → Audio
levels. It lowers excessive software gain before applying it to each block,
aiming for at least 6 dB of headroom. It only reduces gain, so silence does not
increase microphone noise. The new gain is shown and remembered. If the raw
input clips, Tux-sstv also gradually reduces a supported capture-only volume on
the explicitly selected sound card. Choose a named device in Audio devices for
this hardware adjustment; ambiguous `default` routes are left to the desktop.
If the device has no adjustable capture volume, or clipping happens inside the
radio/interface, lower its physical RX/input control. Previously clipped samples
cannot be repaired. Transmit levels and playback controls are unaffected.

Normal reception uses one stream, including while images decode. The monitor
moves into the Radio console and back when switching tabs. The separate input
meter remains a bounded 30-second diagnostic when automatic input is unavailable.
During normal reception, the RX meter is already active on the same stream.

Background receive decoding runs independently of picture editing. Incoming audio
continues while you type or drag text, and RX refreshes do not interrupt a drag.
Unchanged photos and text rasters are reused during movement; final PNGs and
transmitted pixels use the same full-quality composition.

**Freeze** holds the display while reception continues; uncheck it to see the
latest audio again. **Clear** empties display history without stopping RX or
changing a received picture. A new input session clears/unfreezes the monitor.
Stopping input preserves the last display with **RX stopped** shown. Your
Waterfall/Spectrum choice is remembered. Spectrum also supports the existing
WAV/audio preview; the waterfall is reserved for live input.

On smaller windows the side controls scroll, with **Record audio** and **Stop**
kept at the bottom. Reception starts automatically.

## Undo and Redo for TX pictures

Use **Undo** / **Redo** above the TX progress bar, **Edit → Undo / Redo**, or
**Ctrl+Z** / **Ctrl+Shift+Z** (also **Ctrl+Y** for Redo). The same buttons and
shortcuts work in TX fullscreen; both previews also offer Undo/Redo on right-click.
Menu text and button tooltips name the edit that will be reversed.

History covers adding, editing, removing and moving text/emoji; callsign
placement and visibility; template changes/clearing; and received-image inset
additions, sizing, framing, movement and removal. Each drag is one step. Accepted
changes in an editor count as one step; Cancel leaves history alone. Text fields
keep their normal typing Undo/Redo while you are editing them.

The latest **50 edits** are kept for the current picture. Loading another photo
or choosing a test pattern starts a fresh history. Making a new edit after Undo
replaces the old redo branch. Saving a PNG leaves history available, and Undo
updates the TX picture while previously saved files remain on disk. Undo/Redo
are locked during RX, audio preparation, transmission and owned PTT. Source
photos stay intact and each restored state feeds the normal preview/save/encoder
path across the supported SSTV modes. History is cleared when the app closes.

## Picture in picture from a received image

On the **RX** tab, click **Add to TX** to place the received picture inside your
outgoing image. You can also right-click a picture in **Receive history** and
choose **Add to TX as inset…**, or use **File → Picture in picture…**.

Set the inset width with the slider, choose an optional white frame, and drag it
into place in the preview. **Apply** keeps it; **Cancel** keeps the current TX
picture. **Choose saved image…** loads a received picture from disk, and
**Use current RX image** replaces the draft with the picture currently shown in RX.

After applying, drag the inset directly on the TX picture or in TX full screen.
Right-click either TX preview and choose **Picture in picture…** to resize or
replace it. Use **Remove inset** in that editor, or right-click the inset and
choose **Remove received inset**. Text blocks can be placed over the inset.

The inset keeps the complete received picture's proportions and stays as a
snapshot when another RX image arrives. It works over photos and templates and
is included in saved PNGs, TX WAVs and every SSTV mode. The source RX file stays
unchanged. Loading a new TX picture or test pattern clears the inset.

## Save a TX picture

Click **Save image…** below the TX picture, choose **File → Save TX image…**,
or press **Ctrl+Shift+S**. Save a finished PNG with the picture, callsign, text
blocks, emoji and template artwork at the selected SSTV mode's resolution.
Saved pixels match the transmit preview.

The default folder is **Pictures/Tux-sstv/Transmit**. Choose a name and location;
Tux-sstv remembers the folder after a successful save. Existing files require
confirmation before replacement. Saved pictures appear in **Transmit pictures**
for reuse, and your current text blocks stay editable after saving.

In **TX full screen**, right-click and choose **Save TX image…**, or press
**Ctrl+Shift+S**. Saving or canceling returns you to the fullscreen picture.

## Drag the station callsign

Enter your callsign, then click and drag it directly on the **TX picture** to
place it anywhere inside the image. This also works in TX full screen before
sending. The callsign stays inside the picture's edges, and the actual transmitted
image uses that position. The small selection guide appears only in the preview.

You can also move a regular station callsign in **Text & emoji…**. **Apply** keeps
its new position; **Cancel** keeps the previous one. Use **Reset callsign position**
in that editor, or right-click a TX preview for the same reset command. Template
callsigns can be moved on the TX preview or in the Templates editor.

Placement is locked while preparing, recording or sending audio. Positions belong
to the current image: loading a new picture or test pattern restores the ordinary
top-left callsign position. Station callsign text remains in your settings. The
callsign still uses white outlined text with a transparent background.

## Classic test patterns

Click **Test patterns…** below TX or use **File → Test patterns…** for ten built-in
charts: the original Tux-sstv image, SMPTE-style bars, EBU 75% bars, a Philips
PM5544-style circle card, a monoscope-style card, grayscale steps, crosshatch,
multiburst, Siemens star and checkerboard. The picker includes larger previews
and notes on each pattern. The bottom **Transmit pictures** strip is reserved for
photos you load.
**Use pattern** loads the picture and clears the previous image's text/report
fields; **Cancel** keeps the current draft. Sending uses the usual TX controls.
These are RGB drawings adapted for SSTV image comparisons. See
[docs/TEST_PATTERNS.md](docs/TEST_PATTERNS.md) for history, limits and CLI usage.

## Saved template layouts

In **Templates**, choose your artwork and colors and drag the text into place.
Use **Save layout…** to name that layout; saving the same name replaces it.
**My layouts → Load** restores the design, optional photo, colors, caption style,
and text positions. It clears all city/state/weather/temperature/message/destination-callsign/
report/date/time fields and the caption text. Fill in fresh information before
sending. Saved layouts live in the app data folder's `layouts/` directory.
They are applied only when you explicitly load one. **Default colors** restores
the original design palette; **Reset text positions** restores its placement.

## Contact log

Use **Log contact…** below RX to review and save a contact. Callsign and mode
come from the received picture when available. Live receive snapshots the
Hamlib-reported frequency; offline WAVs use Unknown. Date/time and frequency are
editable. Reports and notes start blank. You can attach copies of the current RX
and TX pictures. Only Save creates a contact. **View → Contact log** shows saved
contacts, opens their pictures, and exports CSV. Records and picture copies live
under the app's data folder in `contacts/`; saves are atomic and locked against
simultaneous writers. An unreadable existing log is preserved.

## Received callsigns and replies

A valid legacy FSKID trailer displays the sending callsign beneath the RX picture.
The decoder requires the leader, marker, terminator and XOR checksum. It accepts
up to 12 callsign characters and checks the first five seconds after a legacy
image, including trailers split between capture updates. Missing/corrupt IDs
leave the call blank. The callsign is stored in the auto-saved PNG metadata.
**Reply** starts a fresh Back to You card addressed to that station; other report
fields stay blank. This addition receives FSKID; it does not change TX audio or
add contest-number decoding. Independent on-air verification remains pending.

## Audio setup

Open **Option → Setup wizard**, choose the station connection, then open the
audio check in **Audio devices and levels** (also accessible through **Radio →
Audio levels → Setup wizard…**).
Choose your connection: **SignaLink USB**, **Radio with built-in USB audio**,
**Other USB audio interface**, or **Computer sound card / other device**.
Then select one **Sound device** by its readable name; that choice sets both
receive and transmit. **Refresh devices** rescans after plugging in USB hardware.

For a SignaLink, select its **USB AUDIO CODEC** device. USB radios may also use
that name instead of their model. If several devices share a name, the list
distinguishes their card IDs; unplug/replug and Refresh to identify yours.
The wizard preserves your current choices while scanning, including devices
that are temporarily disconnected. **Advanced** keeps separate receive/transmit
choices and manual ALSA names available. Hardware choices use card names and
prefer ALSA's `plughw` conversion for sample-rate/channel compatibility.

The next pages show the selected device, check receive level/clipping and play
a one-second transmit tone. SignaLink guidance explains the RX/TX knobs; USB
radio guidance points to the radio's USB audio settings. Checks stop on page
changes and close. Audio checks never request Hamlib PTT, but SignaLink or VOX
can key a radio when the test tone plays.

On **Set levels and check transmit audio**, SignaLink users can click
**Optimize SignaLink audio levels**. This unmutes the selected USB card's
playback controls, raises them to normal output (0 dB when supported, otherwise
the available maximum), and sets **Tux-sstv TX level** to 100%. The wizard reads
the controls back before reporting success. It does not change laptop speakers,
default audio routes, receive controls or radio settings, and the button plays
no audio. **Play test tone** remains a separate, explicit action.

**Finish** keeps the selected device's optimized mixer settings and saves the TX
level. **Cancel** restores the original playback volumes/mutes and leaves the
saved TX level alone. If you optimize more than one device, Finish restores the
devices you did not select. A disconnected/replaced device or failed restore
produces an error message. Unsupported mixers and custom/default routes keep
manual setup available instead of guessing a sound card. The TX level can also
be adjusted directly in the wizard. Use the receive meter and the interface's
RX control to set receive level; playback optimization does not calibrate RF power.

For SignaLink, the wizard's tone uses the normal SSTV signal amplitude and the
current TX level, so the test is not attenuated below a normal transmission.
At 0% TX it explains how to raise the level rather than playing a silent test.
Watch the SignaLink's red PTT LED: playback success does not verify keying. If
the LED stays off, check the codec's computer playback mute/volume; the physical
TX knob controls audio sent to the radio, not the SignaLink's PTT threshold.
If the LED lights but the radio does not key, check the radio cable, PTT jumper
or module and the radio's port setup. Tux-sstv cannot read the SignaLink LED.

On the final page, SignaLink users can choose **Let SignaLink handle PTT**. Finish
then turns off automatic Hamlib PTT while preserving the radio connection
settings. Other connection types keep the current PTT configuration. Finish
saves the audio choices, TX level and connection type; Cancel restores your previous setup.
Radio controls can optionally open after Finish.

## Receive correction

On **View → Sync corrections**, enable **Automatically straighten received images**
and/or **Automatically fine tune received tones (AFC)**. These start disabled so
existing manual settings remain in effect until you choose automatic correction.
Slant correction needs at least 12 consistent line-sync pulses spanning three
seconds. AFC uses the leader of a validated VIS header. The receive status shows
the measured values; insufficient evidence retains your manual correction.
AVT has no line sync and uses the manual clock.
CLI equivalents are `--auto-clock` and `--auto-afc` with `--decode`.

## Operating layout

The interface follows a compact, classic SSTV layout: a light gray workspace,
**RX / TX / Sync** image tabs on the left, a spectrum and quick mode buttons on
the right, and a thumbnail strip below. RX/TX mode tabs follow the active image
view; Sync shows receive controls. The full mode dropdown retains all implemented
modes, and the TX progress bar remains directly beneath the sending image.

Press **F11** (or choose **View → Full screen**) to toggle the entire operating
window full screen. Press F11 again to restore its previous normal or maximized
state. RX/TX image-only full screen remains available through its own buttons.

Use **Full screen** below the RX picture to enlarge the receiving image. It keeps
updating during reception and preserves the image's proportions. In Auto receive,
each completed picture is saved to receive history, stays on screen between
transmissions, and is replaced by the next picture as it arrives. The viewer stays
full screen through those changes until you press **Esc** to return to the same
operating layout; reception continues throughout. The mouse pointer is hidden over
the full-screen view and returns normally after Esc. Full screen also works for
completed and saved RX pictures.

The TX tab also has **Full screen**. It shows the sending image with the same
progress bar, percentage, elapsed time and remaining time along the bottom.
The progress bar itself also shows a countdown in whole seconds, for example
`42% · 18 s remaining`. It includes the optional Morse ID, reaches zero at the
end, and shows 100% only after audio playback finishes successfully.
Press **Space** in the TX full-screen view to start sending. Repeated presses and
holding the key do not restart an active send; Space in RX does not start TX.
The pointer starts hidden. Move the mouse to show it for three seconds; each
movement restarts that timer, including over the progress bar. After three seconds
without movement it hides again. The pointer and drag guide are display-only and
are never included in the transmitted image, even if you start TX while they are
visible. **Esc** restores the normal layout without stopping TX. When sending finishes successfully, the TX view automatically returns to the
previous window layout with the completed progress bar.

Use **Templates…** beneath the TX image to create a weather or greeting card.
The **All templates / Weather cards / SSTV exchanges / TV Studio** filter helps browse the
thumbnail gallery. The original weather designs are
**Weather Station**, **Sunset Ridge**, **Aurora**, and **Paper QSL**. Fill in
city, state/region, weather, temperature (°F or °C), optional wind speed,
rainfall, humidity and barometric pressure (BP), date, time, and a greeting.
Wind supports mph, km/h, knots or m/s; rainfall supports inches or millimeters;
pressure supports inHg or hPa (mb). Humidity is 0–100%. Changing a unit converts
an entered reading. Leave a reading blank to omit it; zero wind or rainfall is
shown as zero. The readings appear in two rows below the temperature, and each
can be dragged independently. The editor fields scroll on smaller screens.
Conditions can be selected from the list or typed freely. Blank weather and
temperature fields are marked as not entered rather than invented. Your station
callsign is included automatically. The preview uses the selected SSTV mode's
resolution; monochrome modes show the finished card in grayscale.

Choose **TV Studio** for five original television-style designs:

- **Newsroom** — your headline, story details and an update ticker.
- **Breaking News** — a bold red bulletin with your headline and ticker.
- **Sports Scoreboard** — home/away team names, scores and a score ticker.
- **Now Showing** — a movie or program marquee with details and time/channel.
- **On Air** — a studio title, guest/topic and program information.

Write your own text; story and score fields start blank. The ticker is part of
this still picture. Optional city, date/time and your station callsign complete
the graphic. Use **Background → Choose…** for your own photo, drag text in the
preview, or adjust the colors. These cards support the same Apply/Cancel,
undo/redo, saved-layout and transmit workflows as the other templates. Saved
layouts retain the design, colors, artwork and positions without old stories,
team names or scores. No news or score service is contacted.

The **SSTV exchanges** category contains cards for sending pictures back and
forth during a contact:

- **CQ SSTV** — invite another station to swap pictures.
- **Back to You** — address the other station and send a reply.
- **Picture Report** — send a report about the picture you received.
- **Thanks!** — say thanks for the picture.
- **Your Turn** — invite the other operator to send one back.
- **See You Again** — end the exchange with 73.
- **Picture Swap** — keep your own photo prominent with a callsign/message frame.

Your own callsign is added automatically. **To callsign**, message, location and
date/time start blank and are optional. Picture Report has an additional free-text
**Report** field. The **Quick reply** picker inserts phrases such as “Nice picture!”,
“Your turn!” and “One more before 73?” only when you choose one.
**Background → Choose…** selects your photo; **Use picture**
uses it behind the card. You can also load a picture in TX first. Cancel preserves
the current TX picture; original image files are never overwritten. Weather conditions and
all weather-reading fields are hidden and unused on exchange cards. Weather
cards keep their original layout when the additional readings are blank. The
earlier cartoon-style designs are removed.

Date and time start blank; type YYYY-MM-DD and HH:MM (24-hour), or use the
**Use current date & time** button to fill both fields for the selected Local/UTC time
zone. Switching time zones preserves the same instant; changing °F/°C converts
an entered temperature. All weather readings are entered manually, without an
online service. Unit preferences are remembered; readings start blank for each
new image and are excluded from saved layouts.
Report time stays fixed after applying, so the image you preview is the image
you send. Reopen Templates and refresh the time for a new report.

Drag individual text directly on the **Templates** preview to move callsigns,
location, weather, temperature, dates, greetings, and exchange-card headings.
Hover over a text block to see its editing outline, then drag it to the new
position. **Reset text positions** restores the design's original layout.
Moved text stays inside the image and is drawn above the card artwork.
These placements belong to the current image; a new image or Clear all fields
resets them. Apply keeps the layout, and Cancel discards the changes.

**Use template** applies the design to the current picture; **Cancel** discards
edits. **Clear all fields** empties the form, including date, time, greeting and
caption; Use template applies that cleared draft. The current image's applied
fields remain available when reopening its editor, so it can be edited.

New pictures, a fresh template, and app launches start with empty image fields
and caption text. Loading a picture, selecting a stock thumbnail, choosing a test pattern,
or **Use picture only** clears the old image's text and report fields. Report
content is never saved in application settings; older saved content is removed
when the app opens. Callsign, radio settings, design/unit/zone preferences,
caption styling and recent emoji are kept.

Text/emoji captions also work over templates. Cards become normal image pixels
in TX, full screen and WAV export; receivers need no template support. These
built-in designs are drawn locally and need no downloaded art or extra packages.

Text, emoji and their outlines render at four times the image resolution and
are smoothly reduced before compositing. The background stays at its fitted
resolution, avoiding repeated resampling or marks left behind when text moves.

Use **Text & emoji…** below the TX image to add a new text block. The message
starts blank every time; blocks you have already applied stay on the picture.
Type your message or add just an emoji, place it, then **Apply**. Open the editor
again to add another block. Each block keeps its own text, size, color and position.
The live preview shows all placed blocks at the selected SSTV mode's resolution,
including the station callsign. Each block supports up to six lines and 500
characters, nine position presets, bold text, a black outline and an optional
dark background. Text shrinks to fit when necessary.

After applying a caption, drag it directly on the **TX picture** or in **TX full
screen**, just like the callsign. This also works over templates and when no
callsign is entered. Each block and the callsign move independently. You can also
drag the caption on the **Text & emoji** preview.
Its text and emoji move together as one block. The Position menu switches to
**Free position (drag)**; choosing one of the nine presets restores an anchored
placement. The preview accounts for image borders and letterboxing, and keeps
each block inside the picture, including right up to its edges. Blocks may
overlap other blocks and the callsign. Dragging a block brings it to the front;
new blocks also start on top. At an overlap, click and drag the visible top block.
Apply uses the same placement in TX, full screen and saved/transmitted audio. The editing
outline appears only in the previews and is never transmitted. A new image clears all text blocks and their positions.


The emoji picker includes search, categories, recent picks, flags and skin tones
for supported hands and people. Click an emoji, or select it and press Enter, to
insert it at the text cursor. You can also paste Unicode text and emoji. **Apply**
adds the new block; **Cancel** leaves the outgoing image unchanged. Applying a
blank new message adds nothing. To change something already placed, right-click
that block in TX or TX full screen and choose **Edit text & emoji…** or **Remove
text block**. While editing an existing block, **Clear caption → Apply** removes
only that block. Captions belong to the current picture and clear
for a new image or app launch. Caption styling and recent emoji are remembered.

Text and color emoji become ordinary image pixels in the preview, full-screen TX,
saved TX WAV and transmitted audio. Receiving stations need no emoji fonts. B/W
modes show the complete caption in grayscale. Original source pictures are never
overwritten. Ubuntu's `fonts-noto-color-emoji` package supplies the color glyphs.

Use **Option → Settings** for callsign and audio input/output, or edit them in the
Station/audio panel. **Transmit pictures** shows only your loaded photos. Click
**Add photos…** in that tab or **Load picture…** under TX; you can choose several
images at once. All readable selections are added to the saved list, with the
first one loaded into TX. Selecting a thumbnail loads that picture.

Use **Screenshots…** beside **Add photos…**, or **File → Load screenshot…**, to
choose a saved screen capture. The picker opens Pictures/Screenshots, highlights
the newest image, and includes that folder in its sidebar. Choose a capture and
press Open to load it into TX and add it to Transmit pictures. A different folder
is remembered after selecting an image there. If the screenshot folder is absent,
the picker starts in Pictures. Cancel keeps the current image and folder choice.
Take the screenshot using your desktop's usual screenshot tool first.

Use **Take selfie…** beside Screenshots, or **File → Take selfie…**, to open your
webcam. Select a camera if needed, wait for the live preview, and press **Capture**.
Capture freezes the picture and turns off the camera. **Retake** opens it again;
**Use photo** saves a PNG in **Pictures/Tux-sstv/Selfies**, loads it into TX and
adds it to Transmit pictures. **Mirror photo** applies to both preview and saved
picture. Cancel closes the camera and saves nothing. Camera/mirror choices are
remembered only when you use a photo. Automatic receiving resumes after closing
the selfie window.

The webcam preview/capture is 640 × 480, with source aspect ratio preserved.
Linux V4L2 webcams use GStreamer; the installer includes its tools and base/good
plugins. Close another app using the webcam if it reports busy; Refresh also
finds a newly connected camera. The camera opens only from Take selfie.

Select a photo and click **Remove selected**, use its right-click menu, or press
**Delete** while the list has focus. Ctrl/Shift selects several photos for batch
removal. This removes entries from the list and keeps the original files and
current TX image. The list is remembered between launches. Built-in test images
remain in **Test patterns…** and are never added to this strip automatically.
The photo controls are locked while receiving, preparing or sending audio.

**Receive history** shows thumbnails of up to 256 saved images; double-click one
to open it in RX. Older image files remain on disk.

**Option → Settings → Appearance → Skin** offers twelve built-in looks:

- **Classic** — the familiar gray station with beveled buttons.
- **Midnight** — navy panels with ice-blue accents and a cyan spectrum.
- **Aurora** — violet and lilac controls with a mint spectrum.
- **Ember** — charcoal, copper and warm amber.
- **Phosphor** — a green CRT look with monospace lettering.
- **Arctic** — bright panels with slate text and cobalt accents.
- **Neon Circuit** — carbon black, electric cyan and acid-lime highlights.
- **Synthwave** — ultraviolet, hot pink and turquoise with neon panel edges.
- **Solar Flare** — graphite metal, orange controls and warm gold details.
- **Ocean Glass** — rounded ocean-blue glass panels with seafoam accents.
- **Titanium** — silver instruments, graphite lettering and precision blue.
- **Crimson Ops** — angular obsidian panels with ruby and silver highlights.

The chooser includes two-color swatches and a compact material preview. The six
new skins add gradient panels/buttons, accented tabs, sliders and progress bars.
The Radio console, tuning dial and audio meters now follow your chosen skin;
Classic keeps its familiar dark radio console. Meter warning colors remain
recognizable in dark and light skins.

Changes preview immediately. **Save** remembers the skin across launches;
**Cancel** restores your previous choice. Classic remains the default. Skins also
style the spectrum and progress display, while SSTV pictures retain their original
colors in previews, saved files and full screen.

The **Station / audio** panel has separate **Callsign on image** and **Morse ID**
checkboxes. For Morse-only identification, **uncheck Callsign on image** and
**check Morse ID**, keeping your callsign in the Call field. Hiding the automatic
callsign applies to photos, templates, editors, fullscreen, saved TX pictures and
transmitted images. Your stored callsign and its placement are kept. The same
options are available by right-clicking either TX preview. These settings are
remembered across launches; they stay locked during receiving/preparing/sending.

In **Option → Settings → Callsign identification**, enable **Send callsign in Morse
after TX** to send your station callsign once after each SSTV image. It defaults
to off, with **20 WPM** and an **800 Hz** audio tone. Speed is adjustable from
5–40 WPM and tone from 400–2500 Hz. Callsigns use letters, digits, `/` and `-`;
portable suffixes such as `/P` are included. A missing or unsupported callsign is
flagged before transmission.

The ID is included in TX audio and saved TX WAVs, after a half-second quiet gap.
The progress bar includes its duration and shows **Sending Morse ID** during that
part. TX full screen returns to normal after the image and Morse ID finish.
Settings are remembered across launches. Command-line encoding does not read the
GUI's Morse preference.

## Build and test

Dependencies on Ubuntu:

```bash
sudo apt install build-essential cmake qt6-base-dev alsa-utils python3 fonts-noto-color-emoji libhamlib-dev libhamlib-utils gstreamer1.0-tools gstreamer1.0-plugins-base gstreamer1.0-plugins-good
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j 4
ctest --test-dir build --output-on-failure
```

Qt Concurrent and Qt Test are provided by the Qt base development packages.
Python is used only by simulated audio processes in tests. A production
build can omit tests with `-DBUILD_TESTING=OFF`. The runtime uses Qt 6 Widgets, Hamlib 4 and
ALSA's `arecord` / `aplay`; Qt Multimedia is not required.

An optional user-local install is available with:

```bash
cmake --install build --prefix "$HOME/.local"
```

## Debian package

Build a native `.deb` for the Ubuntu/Debian release used to compile it:

```bash
./packaging/build-deb.sh
```

Packages are written to `dist/`. Building this release on Ubuntu 26.04 amd64
produces `tux-sstv_1.14.0-1~ubuntu26.04_amd64.deb`. The earlier 1.13.0 installer
was built with Qt 6.10 for that release and is not intended for older Ubuntu
versions. Build on the target release to generate its dependencies; changing a
package filename does not make it compatible with another distribution.
The package installs the application, desktop launcher, icon, documentation and
third-party license notices.

To install the package through APT:

```bash
sudo apt install ./dist/tux-sstv_1.14.0-1~ubuntu26.04_amd64.deb
```

The current workstation also has a user-local release installation at
`~/.local/bin/opensstv` and an application-menu entry. Its desktop icon runs
`packaging/pin-to-dock.py` before opening the application; that helper adds
Tux-sstv to GNOME favorites and preserves existing favorites. It requires access
to the normal desktop session. If needed, run the helper directly from a terminal:

```bash
./packaging/pin-to-dock.py
```

## Hamlib radio control

Open **Option → Settings → Radio (Hamlib)**, enable Hamlib, and select the radio
model, device path and baud rate. Choose CAT PTT, serial RTS/DTR or the appropriate
supported PTT interface. An existing rigctld server can be used instead. Save,
then choose **Radio → Connect**. Startup never opens the radio automatically.
The connection check reads state without requesting PTT ON.

The **Radio…** button opens frequency, operating mode/bandwidth, VFO, RF power
percentage, RIT, split TX frequency, repeater shift/offset and CTCSS TX controls.
Each Set button changes its own row. Unsupported operations display Hamlib's
error; hardware and backend support vary.

With automatic PTT enabled, Play TX prepares the complete audio first, waits for
Hamlib to acknowledge PTT ON, observes the lead delay, plays the image and
optional Morse callsign, observes the tail delay, then requests PTT OFF.
Successful TX reaches 100% and exits TX full screen only after PTT OFF succeeds.
Stop, audio errors, connection faults and close also attempt PTT OFF; a duration
watchdog stops a hung player. Failed automatic PTT never falls back to manual TX.
Saving a TX WAV never keys the radio.

See [radio setup and recovery](docs/RADIO.md) for direct/server examples and PTT
failure behavior. Hamlib's Dummy backend and simulated failure tests pass;
physical radio/interface testing is still required.

## Modes

Implemented TX/RX:

- Robot 24, 36 and 72; B/W 8 and 12; AVT 90.
- Scottie 1, 2, DX; Martin 1, 2; SC2-60, SC2-120, SC2-180.
- PD50, 90, 120, 160, 180, 240, 290; Pasokon P3, P5, P7.
- MP73, 115, 140, 175; MR73, 90, 115, 140, 175; ML180, 240, 280, 320.
- MP73-N, MP110-N, MP140-N; MC110-N, MC140-N, MC180-N.

All 43 modes in the MMSSTV target catalog are selectable for TX and RX. Tests
against our own encoder and independent specification fixtures are implementation
evidence; they are not MMSSTV interoperability evidence.

AVT 90 implements the normal RGB format with its complete 32-word digital control
header. The receiver accepts both the published marker and the reported MMSSTV
legacy marker, and consistent ascending or descending counters. It requires a
consistent pair of valid complemented words; it can skip damaged words. If the
VIS header is missing, force AVT 90 to recover from the remaining countdown. For
image-only audio, select AVT 90 and enter its manual image start. Interlaced and
narrow AVT variants are outside the single AVT 90 profile in this target.

B/W 8 and B/W 12 use a 7 ms sync and full-range luminance. Their TX previews are
grayscale. Timing profiles and known differences are recorded in
[docs/SOURCES.md](docs/SOURCES.md).

## Receiver audio and playback

Find ALSA PCM names with `arecord -L` and `aplay -L`. The input and output fields
accept names such as `default` or `plughw:CARD=USB,DEV=0`. Using `default` follows
the desktop sound configuration. `plughw` lets ALSA convert rates for a device.

**Auto detect · All modes** is the default RX selection. Reception starts
automatically on the selected input. The receiver checks for a mode header approximately
every second and shows **Auto detected · [mode name]** above the RX picture as soon
as it recognizes one, including AVT before its countdown finishes. Valid standard,
extended and narrowband headers identify the 43 supported modes. Noise, invalid
headers and unsupported identifiers leave Auto searching. Catch the beginning of
the transmission: this identifies headers, rather than guessing from picture audio.

Auto reception is always enabled while the application is idle. It starts on
launch, stays ready between images, resumes after Stop/TX/dialogs, and reconnects
automatically after input errors. No Auto selection or Record audio step is needed.
Selecting Auto while it is already listening preserves any header or image in
progress. Choose a specific mode only when you want a manual override.

The receiver opens mono, signed 16-bit, 12000 Hz audio automatically. Legacy pictures build as rows
arrive. **Every decoded image saves automatically**, in both Auto and manually
selected modes, in the normal layout and full screen. The first decoded rows
create a PNG under your Pictures directory in `Tux-sstv/`; further rows update that
same file atomically. Each new transmission gets its own file, even if its picture
or mode matches the previous one. Partial images remain saved if you stop or close
the program. Blank placeholders and audio with no decoded image do not create files.

The RX panel shows the automatic save location or a persistent write error.
**Save copy…** exports an additional copy when needed. Opening an RX WAV also saves
its decoded picture, including a partial image from a truncated recording.

After an image completes, the receiver shows **Ready for next transmission** and
keeps listening on the same audio input. Each new image gets a separate saved
file. A fresh validated header also starts a new picture if the previous
transmission ended early; its saved partial picture stays in history. Auto can
select a different mode; manual selection keeps the chosen mode. A manual image-start
offset applies to the first image in that session. **Record audio** is optional;
**Finish recording** saves its WAV while automatic reception continues. **Stop** cancels the current operation and discards an active
WAV recording; saved pictures remain in history. WAV recordings are limited to ten
minutes; automatic reception continues without that session limit. Selecting a specific RX mode overrides Auto; the manual image-start
field is available only with a specific mode selected.

**Play TX audio** generates and plays a 48000 Hz WAV through the selected output.
The large progress bar beneath the TX image shows audio preparation, playback
percentage, elapsed time, and estimated time remaining. It freezes on Stop and
reaches 100% only after successful playback completion. Playback timing uses an
elapsed-time estimate rather than sound-device sample-position feedback.

Hamlib automatic PTT is optional; disable it for manual PTT/VOX operation. No
radio opens automatically at startup. Tests use Dummy/fake radios and simulated
audio processes, without transmitting RF or opening physical audio devices.

WAV input supports 16-bit PCM mono/stereo at 8000–96000 Hz. Left is the default
channel; right and stereo average are selectable. Other formats should first be
exported as PCM WAV. Automatic receive identification supports standard VIS with
even parity, JE3HHT's 16-bit extended VIS, and narrowband N-VIS with its prefix and
XOR check. Manual mode/start, frequency offset and sample clock corrections are
available on the **Sync** tab. A forced mode still uses the header timing
unless a manual start is provided.

## Data locations

- Receive images, including partials: your Pictures directory, under `Tux-sstv/`.
- Receiver recordings: Qt's application data directory, normally
  `~/.local/share/Tux-sstv/Tux-sstv/recordings/`.
- Contacts and their picture copies: application data directory, under `contacts/`.
- Reusable blank template layouts: application data directory, under `layouts/`.
- Settings: normally `~/.config/Tux-sstv/Tux-sstv.conf`.
- Playback WAVs: a temporary directory removed when the application closes.

Recording and image histories are retained until you delete them. Opening a WAV
containing multiple transmissions decodes its first supported header/frame; live
receive accepts consecutive complete frames in Auto and manual modes. Opt-in
automatic clock correction, AFC and FSKID receive are tested with synthetic
fixtures. Independent MMSSTV recordings and physical-radio validation remain
outstanding.

## Command line

```bash
build/tux-sstv --list-modes
build/tux-sstv --pattern build/pattern.png
build/tux-sstv --encode build/pattern.png --mode scottie1 --output build/tx.wav
build/tux-sstv --decode build/tx.wav --output build/rx.png
build/tux-sstv --decode receiver.wav --mode martin2 --start-ms 917.5 \
  --clock-ppm 80 --offset-hz 15 --channel right --output picture.png
```

Use `--partial` to inspect incomplete recordings. Failures return a nonzero exit
status. `--help` lists all options. CLI encoding/decoding selects Qt's offscreen
platform so it also works without an active desktop.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the modem design and
[docs/SOURCES.md](docs/SOURCES.md) for protocol references and known discrepancies.
