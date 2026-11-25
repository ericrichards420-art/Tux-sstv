Current release: Tux-sstv 1.14.0 — Final Release. MCA has been removed; entries below also record earlier development.

## Automatic live Morse callsign reception (1.14.0)

Add a bounded streaming CW reader to the receive worker, with automatic tone
selection, coherent-signal/noise rejection and timing fitting for 5–40 WPM.
Show decoded text, estimated tone/speed and an explicit Use callsign action
in RX. Preserve FSKID until the user confirms a Morse candidate; attach its
source to PNG metadata and keep Reply/Log contact using the confirmed call.
Use the same PCM and worker on local/shack and remote clients, with no added
capture/playback or PTT. Exclude known SSTV audio by absolute capture spans,
including image/CW boundaries inside one chunk. Test an independent waveform
matrix, noisy/uneven keying, the existing Morse transmitter, full alphabet,
noise/silence/steady-tone rejection, FSK rejection, successive image/CW
transmissions, bounded state and cancellation. Exercise actual local capture
and remote packet paths plus explicit callsign confirmation and metadata.
Retain 1.13.5 capture-stall recovery and previous receive fixes.


## Automatic receive after a clipped opening tone (1.13.5)

After a report that rebooting the shack restored reception, reproduce a
capture helper that remains alive but stops sending PCM mid-frame. Existing
retries handled process exit but left this case stuck. Monitor elapsed time
since actual PCM, kill the stalled local/shack helper after eight seconds,
and use the existing automatic retry/reset path. Preserve strict header
validation when the laptop resumes on the restored shack stream. Verify
local and remote image reception recovers without controls or PTT, and that
ten seconds of valid silent PCM does not restart the input. This repairs a
specific recovery gap; the report alone cannot identify the hardware cause.

Reproduce 86 missed acquisitions: late input opening and a muted opening
leader across each of the 43 modes. The mode words were complete and valid,
but the detector demanded more preceding leader than had reached the input.
Validate a shorter remaining leader while still requiring the complete mode
word and its parity/checksum. Keep full-preamble validation for replacement
of an active frame. Add rejection tests for missing mode bits, invalid parity
and N-VIS checksum, plus consecutive local and remote receptions using odd
packet boundaries. Measure AFC from the remaining validated leader rather
than earlier missing or muted audio; eight independent waveform cases verify
both offset directions for standard and narrow headers, with and without
preceding silence. This is a reproduced software failure, not proof of the
cause of every missed on-air image.


## Receive-header dropout tolerance (1.13.4)

Remove the receive suspension from Take selfie and Screenshots. Both dialogs
are picture-only operations; stopping RX lost incoming headers/frames and
stopped laptop listening. Preserve the existing input process and decoder
state throughout preview, capture, acceptance and cancellation. Regression
tests reproduce both pauses before the fix, check ongoing remote speaker PCM
without restarting playback, and decode two live images during each local
dialog. Audio setup and transmission retain their intentional pauses.

Reproduce missed valid headers when a 2–4 ms audio dropout coincides with a
single center measurement. Vote across the interior of each header symbol and
control tone instead. Keep parity, checksum, supported-mode and full-preamble
checks intact; use a narrow window for the short leader break. Verify ten
waveform fault cases, rejection of invalid parity despite a dropout, and
consecutive automatic live images through a fake capture process.

The defect was reproduced synthetically. It does not establish the cause of
every missed on-air transmission, and the recent artwork updates did not alter
the receive modem. Real station confirmation remains separate.


## Complete federal holiday artwork (1.13.3)

Add seven original Tux radio pictures to cover all 11 annual U.S. federal
holidays in Holidays. Keep Easter and Halloween, bringing the holiday gallery
to 13 images, alongside the existing 14-image Tux gallery. Use full-size
originals in every mode, preserve the existing pictures unchanged, and verify
federal-holiday coverage against OPM in the gallery tests.


## Holidays gallery (1.13.2)

Add a separate Holidays tab with six full-size seasonal radio pictures.
Keep the 14-image Tux gallery separate. Both galleries retain full originals
when switching modes, use cached thumbnails, and allow personal greetings
and callsigns. Preserve all 1.13.1 PTT fixes and existing station settings.

## Full-size Tux gallery and PTT reliability (1.13.1)

Include 14 original 1402 × 1122 Tux radio cartoons in a separate Tux pictures
gallery. The images are embedded unchanged and remain available without the
developer's Pictures directory. Cache only gallery thumbnails; load the full
original when selected and compose each SSTV mode from that source. Personal
photos and receive history keep their existing storage and removal behavior.

Adopt the independently reviewed 1.13.1 maintenance fixes: guard remote voice
startup against connection loss during status publication, retain unconfirmed
PTT-OFF failures across repeated disconnect/shutdown, bind delayed local and
remote PTT tests to their own activation, and add the duration watchdog to
manual/SignaLink playback. Explicit Stop remains unconditional. Use matching
1.13.1 versions on the shack and laptop for the remote PTT protection.

Install the new quickstart, artwork provenance and third-party notices with
the dependency license texts. Keep Eric Richards (KI5BZO) in About and metadata.

## Named radio profiles (1.13.0)

Add a stable profile selector and manager to the Radio tab, plus a Radio menu
entry. Preserve the current setup as the first profile. Save as new copies and
activates a setup for another radio; rename, save, use and delete inactive
profiles are available. Setup active radio links directly to the guided wizard.

Profiles keep CAT/PTT configuration, RX/TX devices, interface type, application
TX/RX levels, clipping control and receive calibration together. Autosave is
debounced, while switching captures outgoing edits immediately. CAT disconnect
and persistent storage must succeed before another profile becomes active.
Failures keep the previous profile and never start a new CAT connection or PTT.
Automatic RX resumes on the selected input. Metadata edits preserve RX/listening.

Remote selection manages the shack's profiles; the laptop's USB radio settings,
microphone, speakers and pairing remain separate. Its SSTV TX percentage follows
the shack profile without overwriting the saved local value. Active-profile and
revision checks reject stale edits, and a profile change cancels pending laptop
encoding. CAT setup dialogs identify the profile they were opened for. Radio commands,
voice PTT and SSTV upload/commit also carry the intended profile ID, so delayed
Wi-Fi requests cannot operate a newly selected radio. Stop remains unconditional.

The bounded, versioned store validates names, identifiers and all setting types.
Corrupt saved data gets a recovery copy before migration from current settings.
Ordinary radio polls leave the profile selector intact. Tests cover full settings
round trips, migration, outgoing autosave, restart, CRUD, failed disconnect/write,
PTT gating, remote ownership, stale edits, TX-level restoration and stable UI.

All 26 CTest suites passed (641 Qt test cases passed, zero failures,
2 restricted-loopback TCP cases skipped). Authenticated in-memory tests
verify remote ownership and delayed voice/upload/commit requests are rejected
before hardware callbacks. Native fake-radio tests cover failed disconnect and
settings-write rollback without RF. The Radio tab preview was inspected, and the
packaged binary passes version, GUI launch, desktop-file and AppStream validation.

## Full Unicode emoji picker (1.12.0)

Replace the small hand-picked palette with the complete Unicode Emoji 17.0 RGI
set: 3,944 emoji plus nine components. The embedded catalog supplies English
CLDR names and keywords, ten categories, country and subdivision flags,
professions, families, accessibility, gender and mixed-tone sequences. Existing
radio/ham and hello/goodbye search aliases remain available. Browsing is offline.

Default or preferred-tone browsing keeps the grid manageable. All skin tones
exposes every exact sequence, including mixed-tone couples and handshakes.
Tone lookup selects valid catalog entries rather than inserting a modifier into
UTF-16 text, preserving variation selectors and joined multi-person sequences.
Recent stores and searches the actual inserted form. All skin tones is persisted.

The picker batches layout and suppresses intermediate repaints during filtering.
Noto Color Emoji remains the artwork source. A shared shaping fix bypasses Qt's
older emoji parser for explicitly formatted emoji spans, preserving all sixty
new mixed-tone wrestling ligatures in the picker, editor and transmitted image; the installer requires version
2.051 or later to render all Emoji 17.0 sequences. Unicode data notices ship in
both the package and source backup. A maintainer-only script reproduces the
catalog from installed ICU 78.2 and English CLDR annotations via IBus; the app
and its normal build do not depend on Python, IBus or network downloads.

Validation checks the exact normative RGI set fingerprint, uniqueness, categories,
valid skin-tone choices, keyword/alias search, flags and mixed-tone keyboard
insertion, recent search and color rendering in transmitted images. Every catalog
entry is checked through the Qt text shaper for a single complete emoji glyph.

All 25 CTest suites passed (626 Qt test cases passed, zero failures,
2 restricted-loopback TCP cases skipped). Every one of the 3,953 entries
shapes as a single glyph with the installed Noto font. Catalog regeneration is
byte-identical. The offscreen picker preview was inspected, and the packaged
binary passes version, GUI launch, desktop-file and AppStream validation.

## Confirmed radio modes and USB microphone PTT (1.11.0)

Fix TS-990S USB-D1/D2/D3 and LSB-D1/D2/D3 parsing/display for Hamlib versions
whose generic name helpers omit these public modes. Mode selection applies
immediately, waits through a busy CAT worker, keeps only the latest waiting
selection and preserves newer typed drafts. Stop, TX, disconnect or configuration
changes discard waiting selections. Polls never apply a user command.

Remote controls use the shack's model and actual Main/Sub readback. Successful
setters require fresh matching readback; requested values cached by Hamlib no
longer count as hardware confirmation. Frequency, mode/filter, VFO, normalized
power, RIT, split/TX frequency, repeater settings and CTCSS all use this check.
Unavailable and JSON-null readings cannot confirm a zero-valued request.

CAT input selection now uses Hamlib's mic/data-capable PTT type, including older
saved CAT+Data choices. The TS-990S Use USB/data PTT preset selects the rear DATA
SEND route and automatic PTT without connecting or keying. Explicit reconnect
applies saved changes. Radio-side USB Audio routing and output device selection
remain required; D1 by itself does not select the laptop's transmit audio.

The installed TS-990S driver does not expose M/V or filter width, supports only
RX Main / TX Sub split, and uses an incompatible unbanded tone-enable command.
Unavailable controls are disabled. CTCSS uses the radio's own controls; the invalid
optional query is omitted so it cannot interrupt CAT communication.

Validation includes native Hamlib 4.6.5 against a private TS-990S pseudo-terminal:
all six data profiles, frequency, power, RIT, Main/Sub, split/TX frequency,
MIC/default/DATA PTT commands and rejection of an ignored mode setter. Native
Dummy verifies every generic control. Authenticated in-memory remote integration
checks laptop mode selection, real Hamlib DATA SEND, microphone PCM at the
shack's output, and click-to-unlock. Busy-mode and Stop cancellation regressions
cover deferred user selection. No physical radio or on-air modulation is claimed.

All 24 CTest suites passed (618 Qt test cases passed, zero failures,
2 restricted-loopback TCP cases skipped). The final packaged binary passes
version, offscreen GUI launch, desktop-file and AppStream validation. Hardware
USB routing, physical microphone capture and over-air voice need station checking.

## Stop laptop audio on radio disconnect (1.10.1)

Radio Disconnect now mutes laptop speaker playback synchronously before sending
the remote command. A pending-disconnect guard prevents old receive snapshots
from reopening playback before the shack acknowledges. Afterward, CAT-enabled
stations require actual radio connection as well as Wi-Fi/RX readiness. This
prevents automatic USB capture from restarting speaker audio after CAT closes.
Audio-only stations without CAT still support listening.

Successful radio reconnect from either computer can resume the saved listening
choice. A failed disconnect disables listening and reports the command error.
Remote-station disconnect and connection loss continue to stop playback.

Validation: the new immediate-stop regression first failed against 1.10.0 for
both menu and front-panel Disconnect. It now passes with a delayed shack reply,
continued receive PCM after CAT closes, no speaker reopen while disconnected,
and reconnect from either computer. Additional cases cover radio-state gating,
audio-only stations and failed disconnect requests. All 24 CTest suites passed
(607 Qt checks, zero failures, two restricted-loopback TCP checks skipped).
The packaged binary passes version, GUI launch, desktop and AppStream checks.
Validation used fake audio/radio backends and authenticated in-memory transport;
physical radio and speaker hardware were not exercised for this fix.

## Capture-only microphone check (1.10.0)

Mic check in the Radio tab opens a separate local capture-only dialog. It shows
original input and pre-limiter voice peaks with a −18 to −6 dBFS speech target.
Raw full-scale samples are reported even when reduced software gain makes the
output look safe. Guidance distinguishes silence, quiet speech, good headroom,
high peaks, limiter activity and source clipping, including Ubuntu-level advice
when application gain cannot fix the input.

Gain adjusts live, with rolling peak holds and stable status styling. Stop/Start,
Reset readings, microphone selection and error recovery are available. Use
settings saves the mic/gain; Cancel leaves previous settings intact. The dialog
owns no radio, playback or remote voice path and never saves a recording.

Voice/SSTV transmit controls are blocked while checking. On a remote laptop,
receive decoding continues and listening playback mutes until the dialog closes.
Local automatic RX pauses to free exclusive input hardware and resumes on close.
Stop and application close also end microphone capture.

Validation: all 24 CTest suites passed (603 Qt checks, zero failures, two
restricted-loopback TCP checks skipped). Fifteen new cases cover raw clipping
hidden by low gain, limiter reporting, all guidance states, peak holds, input
failure/recovery, live gain, normal mouse acceptance, cancellation, an offline
laptop, early close and local/remote lifecycle. Fake ALSA and radio backends
confirm no check playback, voice stream or PTT; remote RX continues and speaker
listening resumes. The packaged binary passes version, GUI launch, desktop
entry and AppStream checks. Good-level and clipping views were inspected
offscreen; physical microphone/radio hardware was not exercised for this update.

## Remote listening and voice (1.9.0)

The laptop Radio tab now includes Listen to shack, a local speaker picker and
independent listening volume. Find audio devices discovers both local microphones
and speakers without changing the chosen devices. The existing latched microphone
PTT continues to stream speech to the shack radio. Listening settings persist;
listening starts only after opting in and never opens a microphone or keys PTT.

A separate 12 kHz speaker player consumes a copy of the aligned receive PCM.
An 80 ms starting buffer smooths Wi-Fi delivery, stale backlog is discarded, and
receive gaps re-prime without a voice-TX timeout. Speaker volume changes only that
copy. PTT, SSTV TX and setup mute playback; returning to receive restarts with
fresh audio. Disconnect and close stop the player. Output faults disable listening
without stopping decoding or voice controls. No protocol change is required.

Tests cover speaker scaling, odd sample boundaries, bounded burst catch-up,
receive gaps, device discovery, RX playback while decoding continues, PTT mute,
independent mic levels, receive resume, reconnect, output failure and close.

Validation: all 24 CTest suites passed (588 Qt checks, zero failures, two
restricted-loopback TCP checks skipped). Five new voice/speaker tests passed,
including end-to-end playback through fake ALSA helpers and authenticated
in-memory remote transport. Radio updates do not restart speaker playback or
change the listening control's enabled state/style. Local and remote voice,
PTT release, SSTV, setup and prior anti-flicker tests remain passing. The release
binary passes version, GUI launch, desktop entry and AppStream checks. Its remote
Radio layout was inspected offscreen; physical radio/speaker hardware was not
exercised during this update.

## Photo gallery tooltip removed (1.8.2)

Removed the long “Add your own photos here…” hover popup from the shared
transmit-picture gallery, which appears on both TX and RX tabs. Photo controls,
selection and the gallery's accessible name are unchanged.

## Stable Station / audio devices (1.8.1)

Incoming local PCM replaced recordingInfo with Automatic RX and a long device
name, while Hamlib polling replaced it with radio status. Their different wrapped
heights made the sidebar blink and move. A regression test reproduced the exact
text alternation against 1.8.0 before the fix.

updateRadioStatus now owns every non-recording station summary, combining
Automatic RX and Hamlib/PTT status. PCM only changes that summary while explicitly
recording. Finishing a recording restores the normal summary for local and remote
operation. The summary reserves two lines, with full details in its tooltip;
remote input/output devices have one line each. Both this label and liveAudioState
ignore horizontal size hints so long ALSA identifiers cannot expand the scrolling
sidebar. Recording transitions retain the same device-field geometry.

Local tests stream actual PCM through a fake capture helper while polling a fake
Hamlib backend. Remote tests interleave PCM and changing snapshots with long and
short device names. They check stable text, field positions/sizes, enabled states,
summary paint counts, tooltips and recording transitions while reception continues.

Validation: all 24 CTest suites passed (583 Qt checks, zero failures,
2 restricted-loopback TCP checks skipped). The new local test first failed
against 1.8.0 with Automatic RX replaced by Hamlib status; it now passes with
zero summary paints during unchanged polling/PCM and unchanged field geometry
through recording. The matching remote checks and prior anti-flicker regressions
pass too. The 1.8.1 package passes version, GUI launch, desktop entry and
AppStream validation checks. Hardware RF was not exercised for this fix.

## Radio front-panel refresh (1.8.0)

The Radio tab now presents station connection, CAT controls and latched voice
PTT in one theme-aware console. A larger frequency display has a segmented
signal meter and live filter, RF power and RIT readouts. Missing readback remains
unavailable. A knurled tuning dial, grouped operating controls, clearer button
hierarchy and compact spacing improve the laptop layout.

The frequency display, voice microphone controls and Stop / PTT OFF stay fixed
while tuning, waterfall and audio levels scroll below. Waterfall and Audio levels
buttons jump to their sections; the existing return button restores tuning.
Connection, tuning, M/V, independent Set controls and voice latching retain their
existing behavior. No automatic connection, tuning or PTT is introduced.

Layout tests cover light, dark and neon skins at 1000×720, verify tuning controls
fit, and check fixed readout/PTT positions, section navigation and live telemetry.
Remote status and microphone controls retain the previous anti-flicker fixes.

Validation: all 24 CTest suites passed (581 Qt checks, zero failures, two
restricted-loopback TCP checks skipped). This includes radio command behavior,
M/V, voice latching, remote status/paint stability, setup and image/audio paths.
Light/dark/neon layouts were inspected offscreen. The packaged 1.8.0 binary and
compatibility command report the correct version; its GUI smoke test, desktop
entry and AppStream metadata validation pass. Hardware RF operation was not
exercised during this interface update.

## Stable receive message and shack address (1.7.2)

The remote PCM handler and receive-state handler were writing different text
to the same status label: each audio packet inserted the shack address and an
optional silence suffix, then state refresh replaced that text without the
address. The PCM handler also replaced the remote audio-device summary with a
local capture message. Those competing writers caused the visible blinking.

Remote receive status now belongs to the receive-state path. A short activity
message sits above a fixed shack-address line. Audio packets do not rewrite it
or the device summary. Silence remains visible in the waterfall/audio meters.
Real waiting, recording, transmission, audio-error and disconnect changes still
update promptly; full audio errors are available in the tooltip. Remote recording
shows the actual shack capture source and restores its device summary on finish.

Validation: the new streaming-audio regression reproduces the two alternating
messages against the previous 1.7.1 UI library and passes with this fix. It
checks the address/message and device summary through sound, silence and radio
updates, plus recording, TX, errors, waiting and disconnect. After the initial
audio-meter layout settles, repeated packets cause zero status-label paints;
waterfall history continues growing. The status/monitor preview was inspected.
All seven receive/remote regression suites passed (140 Qt Test checks,
zero failed, 1 real TCP check skipped by the restricted environment).
Release startup, both commands, desktop and AppStream checks passed.


## Steady remote refresh (1.7.1)

Remote updates no longer temporarily enable then disable audio-device, Play,
Record and automatic-mode start controls. Each control receives its final state
once per refresh. Radio RX/TX badge and latched voice PTT styles are reapplied
only when their color/state actually changes. Unchanged radio configuration,
signal-meter readings and TX text regions avoid redundant refresh/repaint work.

Both endpoints suppress identical station snapshots; replies still complete,
audio/heartbeat traffic continues normally, and actual PTT/configuration changes
arrive immediately. The laptop-side fixes remain compatible with older shack
versions. Receive, voice and transmit protocols and timing are unchanged.

Validation: all 24 CTest suites passed (577 Qt Test checks passed, zero
failed, 2 real TCP/rigctld checks skipped by the restricted environment).
The simulated remote session records zero audio-device enable-state changes,
zero RX-badge style changes and zero TX-picture paint events during ordinary
meter updates. Edited frequency stays intact, meters still change and real
TX/RX transitions update immediately. Latched voice PTT retains its style until
release. Duplicate snapshots leave audio, command replies and PTT changes
working. Release startup, compatibility command, desktop and AppStream checks
passed; the physical laptop/Wi-Fi display was not available for visual testing.


## TV Studio templates (1.7.0)

Five new original designs join the existing weather and exchange cards:
Newsroom, Breaking News, Sports Scoreboard, Now Showing and On Air. TV Studio
has its own gallery filter. Headline, details and ticker fields adapt to the
selected card; the scoreboard has home/away names and scores, while program
cards add time/channel information. Photos, colors, station identity and
optional location/date/time use the existing editor controls.

Each text element uses the existing movable-text renderer. All cards scale to
the chosen SSTV mode and go through the normal transmit compositor. Apply and
Cancel preserve draft behavior; undo/redo includes TV fields. Clear/new image
removes old content. Settings and saved layouts keep design preferences and
placement without storing story, program or score content. Existing weather
and exchange category choices retain their indices and designs.

Validation: all 24 CTest suites passed (574 Qt Test checks passed, zero failed,
two real TCP/rigctld checks skipped by the restricted environment). TV checks
cover rendering, field isolation, photos, dragging, clear/fresh fields,
Apply/Cancel, undo/redo and simulated transmit/decode for all five designs.
Weather/news/scoreboard layout round trips retain style without old content.
Template and layout suites also passed a final focused run. All five card
previews and the editor were visually inspected. Release startup, command
aliases, APT description, desktop and AppStream checks passed.


## Expanded weather reports (1.6.0)

Weather Station, Sunset Ridge, Aurora and Paper QSL now accept optional wind
speed, rainfall, humidity and barometric pressure (BP). Wind supports mph,
km/h, knots and m/s; rain supports inches and millimeters; BP supports inHg and
hPa (mb). Unit changes convert entered readings. Blank readings are omitted,
zero wind/rain/humidity are retained, and invalid numeric readings prevent Apply.

Two rows of movable readings fit beneath the temperature. Cards without the
new readings retain their old layout. The editor fields scroll while the preview
and action buttons remain available. Apply/cancel, undo/redo, clear fields and
new-image behavior include all four readings. Settings and saved layouts retain
unit choices and text placement without saving old weather readings. Exchange
cards hide and ignore these weather-only fields.

Validation: all 24 CTest suites passed (565 Qt Test checks passed, zero
failed, 2 real TCP/rigctld checks skipped by the restricted environment).
New tests cover rendering without overlaps, numeric validation, unit changes,
fresh reports, saved-layout privacy, cancel/clear, undo/redo and simulated
transmit/decode. Card/editor previews were inspected. Release startup, command
aliases, APT description, desktop and AppStream checks passed.


## Camera penguin artwork (1.5.1)

The Tux mascot now holds a camera, with a large lens and a shutter button.
Matching vector artwork appears in the app icon, radio header, About and setup
logos. The icon uses a light mint background for contrast at small sizes; the
in-app mascot keeps a transparent background. SSTV picture content stays clean.

Validation: existing branding/developer-credit check and all seven setup
workflow checks passed. App icon and About were visually inspected. Release
startup, both command names, desktop and AppStream checks passed.


## Unified Setup wizard (1.5.0)

The public Setup wizard replaces the audio-only entry in Option, Radio and the
audio levels panel. Station role and remote address/key come first, followed by
the existing device and level checks, an integrated CAT/Hamlib page and a saved
settings summary. Each step identifies the target computer. Remote audio and
CAT reuse the authenticated shack setup operations; local profiles remain
separate. Existing working audio can be kept without replaying a test tone.

Connection and CAT changes save on Next; the guided audio check saves on Finish.
Closing Setup keeps steps already saved. Invalid fields remain on their page,
CAT saves wait for acknowledgement, and setup preserves the existing busy/PTT
checks. Audio tests remain explicit; opening Setup never keys the radio.

Validation: all 24 CTest suites passed (561 Qt Test checks passed, zero
failed, 2 real TCP/rigctld checks skipped by the restricted environment).
Release startup, compatibility command, desktop file and AppStream metadata
checks passed. Physical radio, USB audio and home Wi-Fi checks remain external.


## Tux-sstv name and penguin mascots (1.4.0)

OpenSSTV is now Tux-sstv. Window titles, About, radio/audio controls, desktop
listing, saved-image software tags, command help and the installer use the new
name. Original vector penguins appear in the icon, About, radio header and audio
wizard. Transmitted/received pictures keep their clean content. Eric Richards
— KI5BZO remains the credited developer.

The canonical command and Debian package are tux-sstv. A compatibility opensstv
command, the existing desktop ID and Qt storage identity are retained so old
shortcuts, dock pins, settings, contact logs, layouts, recordings, image folders
and remote keys continue to work. The renamed Debian package replaces/provides
opensstv. HMAC domain strings stay unchanged for older peer interoperability.

All 23 CTest suites passed: 554 Qt Test checks passed, zero failed and two real
TCP/rigctld checks skipped by the restricted environment. New checks verify
mascot/icon rendering, About credit and both command names. A separate seeded
legacy-profile check verified callsign, CAT device/model, remote key/address,
audio choices, history and saved application data. Packaged startup, SVG
assets, command compatibility and software-center metadata passed validation.
Actual USB/CAT, Wi-Fi and RF hardware remain outside these checks.

## Reachable setup and explicit local USB targeting (1.3.4)

Settings and audio/radio setup entries remain reachable. Busy operations produce
an explanation with Stop and open setup or Cancel, rather than silently blocking
access. Stop waits for station idleness and PTT release before reopening setup.
Cancel preserves locked voice PTT. General Settings is available on a paired
shack, reserving the transmitter and linking to dedicated hardware setup pages.

A visible station-connection banner identifies local USB, shared shack or remote
laptop operation. Local USB shortcuts in Settings and offline radio/audio setup
exit remote mode explicitly while retaining the remote address/key. This makes
it possible to configure a radio attached here, including the verified TS-990S
Hamlib entry, without needing an unavailable shack. Settings' General page
scrolls to keep its controls reachable on smaller screens. No connection or PTT
is started automatically.

All 23 CTest suites passed: 553 Qt Test checks passed, zero failed and two real
TCP/rigctld checks skipped by the restricted environment. New coverage verifies
paired General settings, offline recovery into local USB audio/radio setup,
saved remote-key preservation, TS-990S selection reaching a fake backend, and
PTT cancellation versus explicit stop-before-setup. The package passed isolated
startup and metadata validation. Physical USB/CAT, Wi-Fi and RF still need
hardware verification.

## Restore the audio setup wizard while paired (1.3.3)

The wizard stays visible in Option, Radio and a direct Radio-tab button. Device
setup no longer redirects to Wi-Fi pairing or disappears in client/paired mode.
The shack can run its native wizard while paired; the laptop can operate the
same wizard's controls against the shack's devices over the authenticated link.

The shack owns ALSA discovery, capture/playback tests and the existing mixer
transaction. A lightweight client mirrors controls and readings without opening
local audio. Finish saves the shack's choices. Cancel or disconnect restores
provisional mixer changes, including remote reporting of restoration failures.
A shared reservation blocks TX during setup; normal RX resumes afterward.
Local laptop device choices, Hamlib setup and click-to-lock PTT are preserved.

All 23 CTest suites passed: 549 Qt Test checks passed, zero failed and two real
TCP/rigctld checks skipped because this environment prohibits sockets. New
checks cover both wizard entry paths, remote input/output and mixer operation,
Finish, Cancel, disconnect, restoration-error reporting and result RPC lifecycle.
The packaged app passed isolated startup and metadata validation. Tests use fake
ALSA/mixer helpers and an authenticated in-memory link; physical USB, Wi-Fi and
RF performance still require hardware testing.

## Restore accessible Hamlib setup (1.3.2)

Radio setup no longer redirects the remote laptop to Wi-Fi pairing, and it no
longer silently refuses the paired shack. Radio → Hamlib setup provides a direct
entry point. The laptop displays the shack's Hamlib enable/model/device/PTT
configuration and saves it through the authenticated connection. The ordinary
settings dialog links to that page instead of showing disabled local settings.

Setup saves validate typed/bounded fields and the current configuration before
closing CAT and persisting changes on the shack. A shared reservation blocks TX
during the update. Busy TX, uploads, uncertain PTT and stale configurations reject
changes. Pairing and the laptop's local settings stay intact; users explicitly
connect the saved radio afterward. Older or disconnected shacks show a clear
update/reconnect hint. Hamlib remains a linked dependency; the installed catalog
includes the FT-857 driver.

All 23 CTest suites passed: 542 Qt Test checks passed, zero failed, and two real
TCP/rigctld loopback checks skipped by the restricted environment. New checks
cover typed configuration parsing, the installed FT-857 catalog entry, laptop
and paired-shack setup dialogs, saved CAT settings reaching the shack backend,
local laptop settings preservation, busy/stale/malformed request rejection,
and disconnected-host hints. Packaged startup and metadata checks passed.
Actual USB/CAT, Wi-Fi and RF hardware remain unavailable here.

## Click-to-lock voice PTT (1.3.1)

Click PTT once to lock voice transmission on, then click it again to stop. The
button stays red and checked during transmission; its label shows how to stop
or cancel startup. Mouse release and tab/window changes preserve the lock.
Space toggles PTT when the button has focus. Stop, close, connection loss and
audio errors clear the lock. The existing three-minute TX limit still applies;
no active lock is restored after reconnecting or restarting.

Validation covers full mouse clicks, locked streaming after release, a second
click during startup/keying, keyboard toggling, tab/focus changes, forced stops,
disconnects and failed PTT-off recovery using simulated radio/audio hardware.

The voice, remote-station, radio and general UI suites passed: 55 Qt Test checks,
zero failures. Packaged startup passed with isolated settings.

## Voice PTT over home Wi-Fi (1.3.0)

The Radio tab adds a momentary PTT button, a local microphone picker, gain and
mic meter. Holding PTT captures 48 kHz mono PCM on the laptop and plays it through
the shack's configured radio output. Local voice works through the same control.
The mic gain limiter preserves digital headroom. Hamlib PTT is acknowledged before
playback; SignaLink uses its speech-operated hardware PTT and DLY setting.

Signed capability discovery disables voice against older hosts. Stream tokens
and sample offsets reject stale or discontinuous audio. Voice shares the SSTV
reservation and stops on release, Stop from either side, focus/tab changes,
audio errors, disconnect, missing audio or the three-minute hold limit. Delayed
key replies cannot restart cancelled voice. The microphone stays closed at idle;
remote SSTV audio/device settings remain separate. Voice RX to laptop speakers
is outside this release.

All 23 CTest suites passed: 537 Qt Test checks passed, zero failed and two
existing real TCP/rigctld loopback checks skipped because this environment
prohibits sockets. Voice checks cover exact microphone PCM at the shack output,
gain limiting, jitter buffering, audio stalls/errors, stale holds, release during
keying, Stop from either side, focus loss, reconnect without TX restart, older
host capability detection and failed PTT-off recovery. Existing SSTV workflows
also passed. Tests use fake ALSA helpers, a fake radio and an authenticated
in-memory transport; real Wi-Fi, USB audio and RF require hardware testing.

## Bidirectional shared-station operation (1.2.0)

The shack's Play TX control stays available while a laptop is connected. Either
computer can reserve and transmit through the same shack audio and radio. A
local reservation is acquired before encoding; accepted laptop uploads prevent
a local send, and a shack reservation rejects a competing laptop upload.
Stop, errors and completion release the reservation.

Shack-initiated playback streams its played audio to a separate live decoder on
the laptop. It automatically selects the RX page, shows Shack TX monitor and
updates the waterfall and image progressively. Laptop-initiated playback retains
the existing shack monitor. Signed capability negotiation keeps the new monitor
packets away from older clients; both ends need 1.2 for the full workflow.

Network monitor PCM has its own stream identity and sample offset, independent
of ordinary antenna audio. The decoder accepts paced network PCM, drains at
successful EOF and discards pending data on interruption. Antenna RX selections
are preserved, receive history is not filled with local TX echoes, and Stop/PTT
off remains available from either side.

All 22 CTest suites passed: 521 Qt Test checks passed, zero failed and two
existing TCP checks skipped by the restricted environment. New checks cover
streamed decoder EOF, independent RX/TX monitor streams, both reservation
orders, shack-to-laptop progressive RX, reverse handoff, simultaneous requests,
Stop from both sides, full-screen switching and receive-history browsing.
Actual Wi-Fi and RF hardware remain unavailable here. Release startup passed
with isolated settings.

## Remote transmit picture monitor (1.1.1)

When the shack starts playing a laptop's waveform, its RX page automatically
shows a Remote TX monitor. A separate background receiver decodes the played
portion of the uploaded WAV, so rows appear in time with transmission. The
waterfall follows outgoing audio. Normal antenna receive settings do not alter
the monitor, and automatic reception resumes after TX. Stop preserves a partial
picture; a fresh waveform starts a new decode with stale worker results discarded.

The display identifies outgoing audio and does not automatically save it as a
received image. Save copy and picture insertion use the displayed image. Contact
logging offers the monitor as a TX image, and Reply is disabled for this local
preview. Both regular and full-screen RX views update together.

All 22 CTest suites passed: 515 Qt Test checks passed, zero failed, and two
existing restricted TCP checks skipped. New checks cover paced partial rows,
EOF drain, stopped/next transmissions, silent audio and the full remote shack
UI with an unrelated manual RX mode. The Release executable passed its
offscreen startup check. Actual radio and Wi-Fi hardware remain unavailable
in this execution environment.

## Remote shack operation (1.1.0)

The shack shares USB audio and Hamlib controls with one paired laptop over the
home LAN. The laptop retains the normal RX display, editor, transmit controls,
radio console and image history. Receive PCM streams at 12000 Hz; transmit audio
is uploaded in bounded chunks, verified and played locally at the shack after
the complete file arrives. The shack owns PTT sequencing and playback timing.

Connection loss, heartbeat expiry and Stop cancel playback and request PTT OFF.
Reconnection resumes reception without repeating a transmission. Local and remote
audio settings stay separate. SignaLink audio-operated PTT needs no CAT connection.
The Remote dialog shows the shack addresses, port and pairing key.

All 21 CTest suites passed: 508 Qt Test checks passed, zero failed and two TCP
loopback checks skipped because this execution environment prohibits sockets.
Queued in-memory transports exercise the actual authentication/session code;
two real MainWindow instances verify consecutive received pictures, CAT/MV
forwarding, shack-only playback, Stop/disconnect PTT release, reconnection
without retransmission, and operation without CAT. Additional checks cover
failed PTT-release acknowledgments, Stop during an upload, immediate TX retry,
connection loss during preparation and socket write failures. Actual Wi-Fi, USB devices
and RF operation still require testing on the two physical computers. The
packaged Release executable passed its offscreen startup check.

## Developer credit (1.0.0)

Help → About OpenSSTV credits “Developed by Eric Richards — KI5BZO” directly
under the version. The README and AppStream developer name carry the same
credit for source distributions and software-center listings.

## Final release and installer presentation (1.0.0)

The installer now introduces OpenSSTV as a picture communication application for
amateur radio, with clear descriptions of automatic reception, 43 SSTV modes,
guided audio setup, picture editing, webcam capture and Hamlib controls.
Development-build wording has been replaced in the package, README, command-line
help and About dialog. Detailed compatibility evidence remains available.

AppStream desktop-application metadata includes feature descriptions, the desktop
launcher association and a stable 1.0.0 release entry. The Debian package also
includes its desktop launcher and scalable icon. Ubuntu's warning for packages
from third-party sources is controlled by App Center, not the description.

The software-center metadata intentionally has no invented homepage, software
license or hosted screenshot URL. AppStream reports a missing-homepage warning
and a missing-content-rating informational notice; the desktop entry validates.

Release checks: all 19 CTest suites passed (484 Qt Test checks passed, zero
failed, one existing restricted-loopback check skipped). The Release binary
passed its offscreen startup smoke check and reports version 1.0.0. Extracted
package contents include the application metadata, launcher and scalable icon.

# Incremental improvements

Requested sequence: finish and check each addition before beginning the next.

1. Receive automatic slant and tone correction — implemented and checked. Opt-in Sync controls, manual fallback, validated VIS leader AFC and robust line-sync timing fit. Verified: 215 codec checks, 28 MCA checks, 19 existing UI checks and 3 focused Sync preference checks. Includes independent synthetic waveforms, noise, missing sync, partial frames, sequential frames and invalid headers.
2. Audio setup wizard — implemented and checked (5 audio workflow checks and 10 radio UI checks). Device selection, live input levels, explicit output and radio/PTT checks.
3. Received FSKID — implemented and checked (8 FSK waveform/stream/UI checks; 215 codec and 28 MCA checks also pass). Detect callsign and offer it for a reply; never carry stale report fields into a new image.
4. Contact log — implemented and checked (6 contact storage/editor checks; FSK and audio suites still pass). Callsign, time, frequency, mode, report and picture references.
5. Reusable custom template layouts — implemented and checked (5 layout checks, 20 template checks and 10 drag checks). Save style/placement, leave per-image information blank.

No independent MMSSTV recording or physical radio has been tested in this environment. Automated audio uses fixtures; radio tests use Hamlib Dummy/fake transports. Never interpret these as on-air validation.

## Final 0.12.0 regression check

All 11 CTest suites passed: 337 Qt Test checks passed, zero failed, one skipped.
The skipped real rigctld Dummy-server check requires loopback sockets, which this
environment prohibits. The 216 codec checks include mistuned invalid-AVT-countdown
recovery; the 20 general UI checks include correction preference persistence.
Screenshots of the main window and template editor were inspected after the
changes. Old release packages and per-stage source/binary checkpoints are retained
in `dist/` and `dist/checkpoints/`.

## Radio-tab audio levels (0.20.0)

Added saved TX attenuation and live RX gain, separate digital peak/RMS meters,
source/software clipping warnings, microphone/receiver input checks, output test
tones and routing to the existing device wizard. TX gain covers image audio,
Morse ID and WAV export; RX gain precedes live decoding and recording. No AGC
or microphone voice passthrough was added. The app still opens no audio device
on startup, and calibration checks never request Hamlib PTT.

All 16 CTest suites passed: 409 Qt Test checks passed, zero failed, one skipped
(the existing loopback-restricted rigctld Dummy-server check). The 10 focused
audio-level checks include actual MainWindow RX/TX, saved gain, process cleanup,
output level/Morse and BW8/MCA Standard/FM Fast gain roundtrips. The native Radio
audio panel screenshot was inspected. Tests used fake audio helpers and
Dummy/simulated radio backends; no RF or physical microphone path was measured.

## Expanded skins (0.21.0)

Added six skins: Neon Circuit, Synthwave, Solar Flare, Ocean Glass, Titanium and
Crimson Ops. The catalog now has twelve entries, with new material finishes,
two-color swatches, a compact Settings preview, accented tabs and progress bars.
Radio console styling, the tuning dial and audio meters follow the skin too;
Classic retains its dark radio console. Custom instrument palettes keep inactive
segments distinct and warning colors readable on light and dark surfaces.

All eleven targeted UI/radio/audio/image-editing CTest suites passed (148 Qt
Test checks, zero failures). Existing preview/Save/Cancel/persistence and
per-skin template checks cover the expanded catalog. Native screenshots of
main, Settings, Radio and audio panels were exported for every skin, and dark
and light examples were inspected. No radio commands or audio-path behavior
changed, and no physical radio/audio device was operated during verification.

## Live audio waterfall (0.22.0)

Added a 500–3000 Hz audio waterfall with 30 seconds of history, 50 ms frame
steps, time/frequency markers, fixed dBFS colors and a selectable spectrum view.
Freeze holds the display without interrupting reception; Clear resets display
history only. Input sessions clear/unfreeze; stopping input retains the last
history with stopped status. The selected view persists. The single monitor
moves into the Radio tab and back, and microphone/input checks feed it too.
The sidebar now scrolls at small window sizes while RX/Stop stay visible.

All twelve targeted CTest suites passed: 162 Qt Test checks, zero failures or
skips. Fourteen waterfall checks cover six-rate tone accuracy, level/separation,
chunk/frame timing, invalid samples, history limits, freeze during ongoing RX,
input checks/failures, source reset, saved view, tab navigation and minimum-window
layout. Existing UI, audio, radio/PTT and image-editing suites also pass. Native
RX/Radio screenshots using synthetic chirp audio were inspected. No physical
radio or audio device was operated; no signal processing in the decoder, audio
waveforms or PTT behavior changed.

## Status-bar cleanup (0.22.1)

Removed the bottom Development / legacy-mode count button. The compatibility
checklist remains under View → Mode compatibility.

## Picture callsign and Morse checkboxes (0.23.0)

Added independent Callsign on image and Morse ID checkboxes beside the station
callsign, plus matching TX context-menu and Settings controls. For Morse-only
identification, clear Callsign on image and enable Morse ID; retain the Call
field. Hiding the automatic picture callsign keeps its stored value available
for Morse and omits station lettering/placeholders from templates, thumbnails,
editors, fullscreen, saved pictures and transmitted images. Defaults and saved
Morse preferences are preserved. Both controls lock during active RX/TX work.

All eighteen CTest suites passed: 437 Qt Test checks passed, zero failed, one
skipped (the existing loopback-restricted rigctld Dummy-server check). The eight
focused identification checks cover independent settings, restart persistence,
Save/Cancel, context menus/editors, all template station fields and placeholders,
and actual BW8/MCA generated WAVs with no automatic picture callsign and an
intact Morse tail. Main-window and Settings checkbox screenshots were inspected.
Audio/radio tests use fake helpers or Dummy backends; no RF test was performed.

## Three MCA profiles (0.24.0)

MCA now offers Standard, FM Color and FM Black and white. FM Color is the new
name for FM Fast, retaining its waveform and old fm-fast alias/preferences.
The new MCA3 profile converts the composed picture to grayscale, retaining the
FM 16-QAM rate, QPSK header, rate-1/2 FEC, CRCs and audible lead-in. Receiver Auto
identifies the checked MCA3 header and displays profile-specific progress. Both
ends need 0.24+ for the new profile; older Standard/FM Color packets are unchanged.

Grayscale applies to the TX/fullscreen preview, saved PNG, editing previews,
template thumbnails and transmitted/received picture. Color source images and
layers remain intact when switching back to FM Color. Time uses the actual
compressed payload; no universal speed improvement or RF performance is claimed.

All nineteen CTest suites passed: 454 Qt Test checks passed, zero failed, one
skipped (the existing restricted-loopback rigctld Dummy-server check). The twelve
new MCA3 checks cover PCM sample rates, lossy grayscale, independent Python
waveforms, damaged/partial packets, mixed profiles and legacy streams, noisy
listeners, source preservation, GUI TX/Auto RX/save and preference migration.
Existing save/inset/gain/Morse tests add black-and-white cases. Standard and FM
Color 12000 Hz pattern WAVs match 0.23.0 byte for byte, and 0.23.0 rejects MCA3.
A fixed-size header guard was added during Release compiler checks and the
focused MCA/MCA3 tests were rerun. The native grayscale profile UI was inspected.
All audio/radio validation used synthetic fixtures, fake helpers or Dummy
backends; no physical radio link was operated.

## TX picture Undo/Redo (0.25.0)

Added Undo/Redo buttons above the TX progress bar, an Edit menu, TX context-menu
actions and Ctrl+Z / Ctrl+Shift+Z (also Ctrl+Y). The shared controls work in TX
fullscreen. History captures complete document states for text/emoji edits,
position/stacking changes, callsign placement/visibility, template changes and
received-image insets. Each drag or accepted editor transaction is one step;
Cancel and unchanged operations leave history alone. New edits replace the redo
branch. Photo/pattern loads reset history; at most 50 steps are kept per picture.
Saving leaves history available, and Undo restores actual renderer/encoder state
while preserving saved files and current station/Morse/radio/audio settings.

All twenty CTest suites passed: 467 Qt Test checks passed, zero failed, one
skipped (the existing restricted-loopback rigctld Dummy-server case). Thirteen
focused Undo checks cover exact PNG restoration, drag grouping/interruption,
caption edit/delete, visibility and station setting scope, template/inset edits,
history limits/reset/branching, native field Undo, fullscreen shortcuts and actual
MCA Standard/FM Black and white sends after Undo. Existing radio/PTT, codec,
MCA, waterfall, audio, skins, save and drag suites also pass. Normal/fullscreen
screenshots were inspected. Tests used synthetic audio helpers and Dummy radio
backends; no physical radio or audio device was operated.

## Radio memory / VFO button (0.25.1)

Added **M/V** next to the Radio tab's VFO selector. It requests memory mode or
returns to the last observed VFO in the current connection, using generic VFO
when connecting in memory mode. Its highlight follows actual readback, including
front-panel changes. Disconnection clears the remembered VFO. The selector now
also represents MEM and VFO. Switching cancels pending dial steps and refreshes
the frequency/mode editors after success. Existing busy, audio and PTT locks
apply. Unsupported commands remain visible as errors; an immediate read after
an error catches radios that changed state before reporting a failure.

Five new console tests cover switching and remembered VFOs, initial-memory and
reconnection behavior, unsupported/unknown readback, audio/busy/PTT locks and
partial command failures. A real Hamlib Dummy test checks memory/VFO token
readback and recovery, including the Dummy backend's frequency-getter limitation
after accepting MEM/VFO. Tests use simulated radios; physical radio support
depends on the selected radio's Hamlib backend.

All twenty CTest suites passed: 473 Qt Test checks passed, zero failed and one
existing restricted-loopback rigctld test skipped. The Radio tab was visually
checked with simulated readback. No physical radio or audio device was operated.

## Simpler audio setup (0.26.0)

The audio wizard now starts with a radio-connection choice and one readable
sound-device selector for both receive and transmit. SignaLink USB, built-in
radio USB audio, other USB interfaces and computer sound cards have specific
guidance. ALSA descriptions replace raw PCM names in the main selector; physical
capture/playback endpoints are paired by stable card/device identifiers, with
plughw conversion preferred. Duplicate codec names remain distinguishable.
Separate devices and manual names remain in Advanced. Refresh rescans devices
without changing current choices, including disconnected or custom selections.

Receive/transmit check pages show the chosen devices and explain SignaLink's
RX/TX knobs or the radio's USB audio settings. Tests remain explicit, bounded
and stopped by navigation/close. The review page can save SignaLink's own PTT
operation by turning off automatic Hamlib PTT; it keeps the other radio settings.
Other connection types leave PTT as configured. Cancel changes nothing.

Six additional audio tests cover readable discovery, paired routing through
actual fake audio-helper invocations, USB-radio selection, refresh/error/unplug
preservation, duplicate USB names and application-level profile/PTT persistence.
Existing tests retain manual-device, clipping/meter cleanup, output failure and
Save/Cancel coverage. All audio checks in tests use synthetic helpers.

All twenty CTest suites passed: 479 Qt Test checks passed, zero failed and one
existing restricted-loopback rigctld check skipped. The wizard was visually
inspected with simulated USB audio devices. No physical radio was keyed and no
real audio input/output check was run during this change.

## SignaLink keying check (0.26.1)

The audio wizard's fixed 10% test tone could be too quiet to activate SignaLink
PTT even when normal SSTV audio would work. The SignaLink profile now checks
with the normal SSTV amplitude scaled by the current TX setting, including an
explicit muted-level message at 0%. Other profiles keep the low-level test.
Success text distinguishes audio playback from observed hardware keying, and
the help separates an unlit SignaLink LED (power/audio/mixer) from a lit LED
without radio TX (cable/PTT jumper/port setup). Playback status identifies
SignaLink audio PTT rather than incorrectly calling it manual PTT.

The actual saved codec and TX/PTT configuration were inspected. This session
cannot access /dev/snd or the desktop PipeWire server, so the live codec mixer
and physical PTT indication remain unverified. No physical TX was initiated.

All three focused audio/audio-level/radio-UI suites passed: 36 Qt Test checks,
zero failures. The fake playback helper validates the actual WAV sample peaks
for 100% and 25% TX, and 0% does not launch playback. A simulated transmission
also verifies SignaLink status and that no Hamlib connection/keying is required.

## SignaLink playback optimization (0.27.0)

Added **Optimize SignaLink audio levels** and a TX-percent draft control to the
audio wizard. Optimization saves the selected USB card's playback mixer state,
unmutes/raises supported output controls and sets the TX draft to 100%. Mixer
readback must verify the result. It never adjusts a system-default/custom route
or a non-USB laptop sound card. Receive controls and shared capture/output gain
are excluded. No test tone plays until the user explicitly requests one.

Cancel restores the exact prior per-channel volume/mute state. Finish keeps
the chosen card's successful adjustment and restores other touched devices;
repeated optimization retains the original snapshot. Identity is checked before
restoring so a replaced card does not receive another device's settings. Errors,
partial writes and ignored commands are handled with verified rollback, and
restoration failures appear in the main-window status. TX percent is committed
to the existing radio-tab controls/settings on Finish; Cancel keeps it unchanged.

The USB mixer helper and all new audio tests use simulated amixer processes.
No real playback mixer was changed and no physical radio was keyed in this
development session. The computer-side baseline still requires the user's
PTT-light check, and a connected radio's drive remains an interface/radio setting.

All twenty CTest suites passed: 495 Qt Test checks passed, zero failed and one
existing restricted-loopback rigctld check skipped. Twelve added mixer checks
exercise readback/rollback, muted and asymmetric stereo state, rejected/partial/
ignored writes, unsupported controls, default/non-USB exclusion, cancellation
during a write, replacement-device protection, signed USB volume values and
selected-device commit. Application tests also verify TX-level Save/Cancel and
propagation into the Radio tab. The optimized wizard screen was inspected.

## Live audio before recording (0.28.0)

Added a Live audio control and explicit source/state above the waterfall. It
opens continuous input for the waterfall and receive meter before Record RX,
with an opt-in launch preference. Record RX promotes the same stream and keeps
a two-second prebuffer rather than reopening the sound device. Monitoring
resumes after completed receive finalization or TX/settings/wizard pauses, and
Stop turns it off. Input failures stop retries and report the error. An open
stream carrying zeros now shows silence instead of an unexplained empty plot.

Six new simulated-input cases cover independent monitoring and WAV handoff,
complete Robot 36 reception when the header starts before Record RX, silent
input/device changes, TX pause/resume, wizard access/resume and opt-in startup
with failure recovery. The tests verify that RX promotion does not reopen the
input and that the final recording contains the same nonzero PCM. No physical
radio/audio link was operated in this session.

All twenty CTest suites passed: 501 Qt Test checks passed, zero failed, and one
existing restricted-loopback rigctld case skipped. Final monitor/audio-level
checks also passed after the status-caption update. Normal live-monitor UI was
inspected with synthetic input. A supplied receive recording was inspected
separately and yielded a recognizable partial Robot 36 image with manual timing;
that result does not establish automatic header acquisition or radio reliability.


## Automatic receive and clipping protection (0.29.0)

Reception, waterfall, decoding and image saving now start automatically. Removed
the Live audio button and optional startup action. Record audio is only needed
for an optional WAV; it promotes the existing receiver without reopening input.
TX/settings/wizard pauses resume automatically. Stop ends the current session;
relaunch starts reception again. Quiet listening feeds incremental decode with
bounded buffers rather than retaining a full audio recording indefinitely.

Automatic RX clipping protection is enabled by default. Excessive software gain
is reduced before processing the current block, targeting -6 dBFS headroom and
never increasing gain during silence. Source clipping triggers gradual capture
volume reductions on supported explicitly selected ALSA cards, with card-identity
and readback checks. Missing controls/default routes show manual input guidance.
TX playback levels and SignaLink keying settings are unchanged.

All twenty CTest suites passed: 505 Qt Test checks passed, zero failed,
and 1 existing restricted-loopback case skipped. Coverage includes two
consecutive automatic Robot 36 images without Record, uninterrupted recording
changes, transmit/wizard pauses, software headroom and original-source clipping,
capture-only hardware adjustment and unchanged TX levels. The interface was
inspected with simulated live input. Physical radio/audio testing remains for
the connected station.


## Ready for the next transmission (0.29.1)

Fixed two reproducible receive handoff failures: an interrupted picture could
consume the next header as missing rows, and cumulative line-sync adjustments
could make the nominal picture end discard the start of a following transmission.
Completed frames now consume through the actual decoded end. A fresh validated
header can interrupt a partial frame, preserving its checkpoint and starting a
new frame with cleared timing/frequency state. Queued audio and late FSKID remain
available; capture and the waterfall continue without a restart. The interface
explicitly reports Ready for next transmission after a completed image.

Regression cases cover completed images with sync drift and a split next header,
interrupted images followed by same/different/narrow/digital modes, AFC/clock
correction, separate transmissions with a quiet gap, and partial-to-new-image
handoffs through the normal automatic receive UI.

All twenty CTest suites passed: 513 Qt Test checks passed, zero failed,
and 1 existing restricted-loopback case skipped. Both the interrupted
Robot 36 handoff and the completed clock-drift/split-header handoff failed
before their fixes and pass afterward. Automatic UI receive checks also passed
for separate complete transmissions and recovery from a partial first picture.
No physical radio link was operated during these checks.


## Clean text over images (0.30.0)

Caption/callsign fills and outlines now use 4× rasterization with smooth reduction,
without resampling the fitted background or baking old text positions into it.
MCA prefers exact lossless pixels whenever they fit its existing 32 KiB frame.
For detailed photos, it starts at WebP quality 95 and reduces quality only to fit
the frame, replacing the aggressive 3600-byte send-time target. Cleaner images
can take longer to transmit; the shown duration follows the real payload. Packet
layout and legacy receiver quality hints remain compatible.

Checks cover untouched background outside text, no colored fringes in white
lettering, no marks at old positions after moving text, exact captioned artwork
through MCA, and reduced caption-region distortion on a photo-like fixture.
The latter's mean RGB error fell from 7.25 to 3.10 /255 versus quality 25.

All twenty CTest suites passed: 516 Qt Test checks passed, zero failed,
and 1 existing restricted-loopback test skipped. Drag/cancel/undo/redo
checks confirm that saved and transmitted pixels match the preview. A newly
prepared high-quality MCA photo-like packet also decoded completely with the
previous installed 0.29.1 CLI. The final clean-text sample was visually inspected.


## Screenshot folder shortcut (0.31.0)

Added Screenshots… beside Add photos… and File → Load screenshot…. Both open an
image picker in the saved screenshot folder (initially Pictures/Screenshots),
preselect the newest image and expose the folder in the sidebar. Choosing another
folder remembers it for the next import. Missing screenshot folders fall back to
Pictures/home. Accepted selections use the existing photo import path, preserving
files and loading the first readable capture into TX. Cancel leaves the current
picture and saved directory unchanged. The controls follow the existing TX edit
locks, and background reception pauses during selection then resumes afterward.

Validated with an isolated screenshot-folder UI walkthrough: newest capture
selection, imports from the button and File menu, custom-folder persistence,
Cancel, missing-folder fallback, 900×680 layout and recording locks all passed.
Existing pattern/photo-library and TX-image suites passed, along with focused
operating-control and receive/startup checks. No desktop capture or radio PTT
was triggered by the screenshot chooser.


## Take selfie (0.32.0)

Added Take selfie… beside Screenshots and in File. The webcam window includes
camera selection/refresh, live preview, Capture, Retake, Mirror photo and Use
photo. Capture releases the camera and freezes the exact frame; Retake reopens
it. Cancel/close saves nothing and always releases the camera. Use photo saves a
PNG in Pictures/OpenSSTV/Selfies and loads it into TX/the picture library. Missing,
busy or inaccessible cameras and missing/stalled capture helpers report errors.

Capture uses a local video-only GStreamer pipeline with packed 640×480 RGB frames,
so camera preview/capture adds no lossy image encoding or audio recording. The
installer declares GStreamer tools and base/good plugins. Physical camera nodes
were not exposed in this workspace; preview/resource cleanup and import behavior
are checked with simulated camera output and a real GStreamer test-video source.

All twenty-one CTest suites passed: 527 Qt Test checks passed, zero failed,
and 1 existing restricted-loopback test skipped. New checks cover fragmented
frame reads, exact pixels/mirroring, Capture/Retake, camera selection, busy and
missing cameras/helpers, early close, cancellation, PNG/library import, receive
resumption and recording locks. The real GStreamer test source produced exact
packed RGB frames. The capture dialog was visually inspected with test video.


## Auto receive reactivation (0.32.1)

A stopped/failed input left receiveRequested false, so changing RX mode only
changed the decoder selection and did not reopen capture. Clicking an already
selected mode emitted no combo change at all. RX mode selection now explicitly
requests reception through both mode buttons and dropdown activation, including
reselecting Auto after Stop or input failure. An active receive session retains
its input and partially captured header when Auto is reselected. The stopped
receiver hint now points to RX mode selection rather than optional WAV recording.

A new regression first reproduced the failure on 0.32.0: after Stop, clicking
the selected Auto button never reopened input. Integration checks feed complete
Robot 36 headers and two consecutive pictures through the automatic input path
without pressing Record, with gates that verify detection/partial rows first.
They cover already-selected Auto, manual-to-Auto changes, dropdown reselection,
recovery from failed input and keeping the captured header on active reselection.

All twenty-one CTest suites passed: 531 Qt Test checks passed, zero failed,
and 1 existing restricted-loopback test skipped. The new four-case regression
passes with both normal and automatic correction settings. Header detection for
all supported modes, consecutive frames, TX/receive resumption and optional
recording remain covered. No live radio input is accessible in this workspace.


## Continuous automatic receive (0.33.0)

Reception now starts and stays enabled by default. Removed the session opt-in
flag that Stop/input errors could clear. Stop cancels the operation and releases
PTT, then automatically returns to receiving. Device errors retry with a bounded
1–10 second backoff, so reconnecting/restoring the input resumes reception without
Auto/Record selection. TX, modal dialogs and owned/reported PTT still pause input.
Closing stops retries. Auto remains the startup mode; manual mode selection is
an optional override. Existing image completion/rearming behavior is retained.

New controlled-audio tests verify mode detection, decoded rows and two saved
Robot 36 images after launch, Stop, a temporary unavailable device and a mid-header
input exit, without selecting Auto or recording. Additional checks restore a
missing capture helper without a click, bound retry attempts, and ensure closing
prevents further attempts.

All twenty-one CTest suites passed: 532 Qt Test checks passed, zero failed,
and 1 existing restricted-loopback test skipped. Radio PTT sequencing, output
errors, watchdog cancellation and release messages remain intact while RX retries.
Input errors remain visible in the RX status area without replacing unrelated
transmit/PTT messages. Live radio hardware is not accessible in this workspace.


## MCA acquisition fixes (0.34.0)

Two reproduced failures: Standard MCA lost its digital header when the first
250 ms of audio was blanked, and all MCA profiles failed at a 25 Hz frequency
offset even with automatic AFC enabled. Standard now starts with the same audible
500 ms tone as the FM profiles, giving audio-keyed interfaces time to settle.
Automatic MCA AFC now measures that tone and only accepts its correction after
the unchanged digital training/FEC/header checks pass. The payload decoder and
live receiver keep the validated correction. Interruption recovery retains the
next lead-in. Old no-tone packets remain supported; payload format/CRC stay intact.

Standard audio is 500 ms longer; the preview/countdown/WAV duration reflect it.
FM profiles retain their previous transmit waveform. No physical radio hardware
is available in this workspace, so this does not establish RF performance.

All twenty-one CTest suites passed: 551 Qt Test checks passed, zero failed,
and 1 existing restricted-loopback test skipped. New tests cover all three
profiles after 250 ms of input loss, ±25/±125 Hz automatic correction, interrupted
and consecutive corrected frames, tone-only/invalid-header rejection, and old
no-tone packets. Live UI tests save two verified pictures per profile through
shifted/echoed audio with RX gain protection, without Auto/Record selection.
The installed 0.33.0 CLI decoded every new profile, the new CLI decoded the old
profiles, and both FM transmit WAVs matched 0.33.0 byte for byte. Standard now
takes 11.525 s for the built-in pattern; FM Color remains 6.200 s and FM Black
and white remains 6.025 s. Test WAVs/compatibility results are in Downloads/OpenSSTV-MCA-check.


## Remove MCA (0.35.0)

Removed MCA from TX/RX selectors, quick buttons, profile controls, compatibility
view, command-line options and automatic header detection. Removed the digital
modem/compression implementation and direct libwebp dependency. The catalog now
contains only the 43 MMSSTV target modes. Older profile settings are discarded
on startup. Existing PNG pictures and all user recordings remain intact.

Composition, preview/save, text dragging, undo/redo, templates, insets, Morse ID
and PTT tests now exercise conventional SSTV modes. The two modem-specific suites
and their waveform fixtures/benchmarks were retired. Continuous automatic RX and
its input recovery behavior are retained.

All nineteen remaining CTest suites passed: 482 Qt Test checks passed, zero
failed, and 1 existing restricted-loopback test skipped. The mode catalog
and UI expose 43 conventional modes; retired mode/profile CLI requests and an
old waveform are rejected. Removed modem symbols and the direct compression
library link are absent. Settings migration clears the obsolete profile. The
interface was visually inspected. A receive-constructor validation check was
corrected to examine the stored mode instead of its moved-from input string.


## Smooth editing during automatic RX (0.36.0)

Reproduced three editor-disable events in 3.2 seconds of silent automatic RX:
receive workers shared the foreground busy state, clearing drag regions and
ending gestures every second. Live decoding now has its own watcher/cancellation
token, so editing stays available while reception continues. RX image delivery
does not recompose TX, position changes avoid whole-control/duration updates,
and resize relies on existing image scaling at paint time.

Repeated movement no longer rescales the same source photo or rerenders the same
callsign/outline. Bounded caches preserve source mutations, mode changes and the
full-quality text path; cached caption crop bounds avoid rescanning text ink.
A live-audio regression drags across multiple receive ticks while actual SSTV
rows decode and checks that no gesture ends or editing control disables.

All nineteen CTest suites passed: 484 Qt Test checks passed, zero failed,
and 1 existing restricted-loopback test skipped. The live-drag regression
keeps its gesture active across receive updates and checks continued waveform/row
updates. Cache checks cover source mutations, color/mono modes, new callsigns,
returned-image mutation and clean old text positions. Existing TX/save/undo/PTT
checks pass. A pending final capture decode is cancelled before foreground work.

On the same optimized build, 100 warm composition moves using a 4000×3000 source
averaged 2.982 → 0.013 ms for Robot 36 and 13.547 → 0.088 ms for PD290. Silent
automatic RX disabled the editor three times in 3.2 seconds before the change,
and zero times afterward. These measure composition/enable events, not total GUI
frame rates or real-radio performance. Logs are in build/editing-performance.txt.
