# Hamlib radio setup

Use the Radio tab’s **Radio setup…** button, **Radio → Hamlib setup…**, or
**Option → Settings → Radio (Hamlib)**. Hamlib is disabled initially. The app
never connects or keys a radio automatically when it starts. Saved connection
settings take effect on the next explicit **Radio → Connect**.

## Kenwood TS-990S and setup access

The installed Hamlib catalog includes **Kenwood TS-990S** (model 2039). Select
that entry after enabling Hamlib; enter the CAT device on the computer attached
to the radio, save, then use Radio → Connect. Audio setup uses the audio device
on that same target computer.

If setup is blocked, 1.3.4 offers **Stop and open setup** instead of a disabled
menu item. If Settings says **Remote laptop** but the radio is attached here,
use its **Set up this computer's USB radio/audio…** shortcut. Offline remote
setup pages provide the same local USB path. A paired shack can open General
settings and follow the audio or Hamlib setup links without unpairing.

## TS-990S USB/data audio and voice (1.11.0)

Select the TS-990S model and the shack CAT port, then choose **Use USB/data PTT**.
This presets **CAT mic/data selection**, **Data / USB / rear input** and automatic
PTT. Save and reconnect explicitly. This button edits settings without opening
CAT or transmitting. Legacy CAT configurations selecting Microphone or Data now
also use the Hamlib mic/data-capable PTT method, so that selection is respected.
Existing rigctld users must configure mic/data-capable PTT in the server too.

For standard USB computer audio, select **USB-D1** (or **LSB-D1** for the intended
sideband) and configure the radio's DATA SEND source for **USB Audio**. D1 alone
does not select the correct PTT audio route: normal SEND and DATA SEND can use
different inputs. The laptop microphone goes through Wi-Fi to the shack's chosen
USB audio output. Confirm that output is the TS-990S sound card. The radio's
USB input level is a separate adjustment from the laptop's microphone gain.
Use **Mic check**, then click PTT, wait for **Talk now**, speak, and click again.

[Kenwood's USB settings guide](https://www.kenwood.com/i/products/info/amateur/ts_990/pdf/ts990_ft8_settings_en.pdf)
illustrates USB-D1 and the DATA SEND source. Although written for FT8, those
USB routing settings also apply to computer-provided voice audio.
[Kenwood's USB Audio manual](https://kasc.kenwood.com/files/prod/2452/5/TS-990S_USB_Audio_manual.pdf)
describes the input-level controls. Tux-sstv does not rewrite radio menu routing,
choose a sideband for you or force D1 at PTT: existing D2/D3 profiles and manual
routing remain usable.

Mode choices include USB-D1/D2/D3 and LSB-D1/D2/D3, preserving the exact profile
reported by Hamlib. Selecting a listed mode sends it immediately; typed entries
use Set. Updates use fresh radio readback, and unconfirmed settings report an
error with the actual value. A mode selected while CAT is busy waits for the worker; only the latest waiting
choice is applied. Stop, disconnect, configuration changes and transmit activity
cancel waiting choices. Polling does not send mode changes. The laptop uses
the shack's radio model and readback, not its own locally saved model.

With installed Hamlib 4.6.5, the TS-990S exposes Main/Sub rather than VFOA/VFOB.
Its split setter supports RX Main / TX Sub. Memory/VFO switching, repeater controls, tone enable
and filter-width setting are unavailable through this driver; those controls are
disabled instead of suggesting a successful change. Set filter width and CTCSS on the radio. Tone-enable polling is skipped because
this driver uses an incompatible TS-990S command format; it must not disconnect
an otherwise working CAT connection.
Other backends retain their supported controls and report rejected or unconfirmed
requests. Successful CAT/PCM tests do not prove on-air modulation.

## Named radio profiles (1.13.0)

Choose **Profiles…** in the Radio tab or **Radio → Radio profiles…**. The existing
setup is preserved as **Current radio** the first time this version opens. Rename
it, for example **Kenwood TS-990S**. **Save as new…** copies the active setup and
selects that new profile; name it for another radio, then use **Setup active
radio…** to change its sound card and CAT settings. Up to 16 named profiles can
be kept on each computer. Names must be unique. Switch away from a profile before
deleting it; Delete asks before removing an inactive profile.

A profile contains Hamlib model/device/baud or rigctld address, target VFO,
PTT method/input/device and delays, automatic PTT, advanced Hamlib settings,
ALSA RX/TX devices, wizard connection type, application TX percentage, RX gain
and automatic clipping prevention, sample-clock/tone calibration and its auto
options. Callsign, image/editor preferences, microphone, speakers and remote
pairing are independent. Hardware mixer levels continue to use the selected
card's ALSA controls and the existing audio wizard.

Changes save automatically to the active profile; Save current is also available.
A switch saves outgoing edits even before the autosave timer fires, disconnects
the old CAT driver and loads all saved settings together. Automatic RX restarts
on the new audio input without playing a test tone or connecting CAT. Connect
explicitly afterward. Renaming, saving or deleting an inactive entry keeps RX
and the current CAT connection running. PTT, SSTV playback/preparation, recording,
Mic check and open setup prevent switching until the operation is finished.
A failed CAT disconnect or settings write leaves the previous profile selected.

When paired, these controls show the shack's list and change its physical radio
setup. Both computers need 1.13 or newer. The laptop uses the selected shack TX
percentage for SSTV audio it prepares. Its own audio/CAT profile, microphone,
speaker and independent receive-monitor adjustments stay local. Disconnecting
remote operation restores the local profile display and TX percentage. The
shack does not switch profiles merely because a laptop connects or reconnects.
A stale remote edit is rejected if the profile changed; pending laptop encoding
is cancelled if the shack selects another radio.

Profiles survive restart in the existing application settings, alongside the
legacy active audio/radio values. Corrupt collections are kept under a recovery
key before the current setup is retained as a new Current radio profile. Profile
names/lists update only when they change; ordinary CAT polls never rebuild the
selector or issue profile commands.

## Remote operation

In OpenSSTV 1.3.2 and newer, Radio setup stays available while a laptop is paired.
On the laptop it shows and saves the shack's Hamlib configuration. Select the
model and CAT path connected to the shack, save, then use Radio → Connect.
Both computers need 1.3.2 for this setup page. Pairing and local laptop settings
are preserved. Hamlib changes are blocked during TX; saving closes the previous
CAT connection and never keys the radio. See [remote setup](REMOTE.md#hamlib-setup-while-connected).

## Direct connection

1. Enable Hamlib and select Direct radio connection.
2. Search for your exact radio model. The list comes from installed Hamlib's
   `rigctl -l`, including the Dummy test radio.
3. Enter the serial/USB device, preferably its stable `/dev/serial/by-id/…`
   path. Select the radio's CAT baud rate (or Driver default).
4. Select CAT, RTS, DTR or another supported PTT method. A separate PTT device is
   optional. CAT mic/data selection and the transmit audio input depend on the
   radio's Hamlib implementation. Configure the radio's actual audio routing too.
5. Set the control target VFO and PTT delays. Defaults are 300 ms before audio
   and 100 ms after audio. Extra Hamlib settings accept comma-separated
   `name=value` pairs, for example `civaddr=0x94,serial_handshake=None`.
6. Check connection reads radio state without requesting PTT ON. Save settings,
   then choose Radio → Connect. Only one application should own the CAT port.

Your Ubuntu user must have access to the device (serial ports commonly use the
`dialout` group). Audio input/output remains configured by ALSA PCM names on the
General tab; Hamlib does not carry the SSTV audio.

For a local simulation, choose Hamlib Dummy (model 1), leave the device blank
and select CAT PTT. Dummy controls do not operate physical radio hardware.

## Existing rigctld server

Select Existing rigctld server and enter its host and port (default 4532).
OpenSSTV uses Hamlib's NET rigctl backend; configure the actual radio model,
device, baud and PTT method in that server. The direct connection fields are
inactive. A server is useful when another station service already owns CAT.
Audio still requires an appropriate local ALSA device/audio route.

## Controls

The **Radio** tab, Radio… button and Radio menu open the OpenSSTV radio console.
It uses a universal design with a large live frequency display, tuning dial and
step buttons, S-meter, mode/filter, VFO, normalized RF-power and RIT controls.
Radio setup opens the existing Hamlib settings. Connect is explicit; opening the
console never connects, tunes or keys the radio. The console uses the same
controller and connection as SSTV transmission.

**M/V** beside the VFO selector switches to **MEM** and back to the last observed
VFO, including VFO B or a sub receiver. When a connection starts in memory mode,
the return request uses Hamlib's generic **VFO** token. The button's highlighted
state follows radio readback, including front-panel changes. The selector also
shows MEM and VFO readback. The remembered VFO is cleared on disconnection.
Switching cancels queued dial steps, refreshes the frequency/mode editors after
success, and never writes or copies a memory channel. Missing VFO readback,
active SSTV audio, PTT and a busy controller disable M/V. Radios that do not
support this command report their Hamlib error in the console status line.

The dial combines quick turns into bounded requests and retains steps entered
while an earlier tuning request is in flight. Explicit frequency entry needs
Tune; selecting a mode applies it immediately, while a typed mode and other editors use Set. Radio polling preserves unfinished
edits, and reconnection initializes controls from the new readback. Unsupported
readings are shown as unavailable. The S-meter uses Hamlib STRENGTH, in dB
relative to S9, and pauses during PTT. RF power uses a normalized percentage.

Stop / PTT OFF remains available while transmitting, including manual SSTV audio
without a Hamlib connection. Settings are locked during active SSTV operations;
frequency, mode and other CAT changes also lock while PTT is active.

**Advanced controls…** opens the previous controls panel. Copy radio values
fills its editors; polling does not overwrite your edits. Each Set button writes only that row. Available rows are frequency,
mode/bandwidth (0 uses the driver's default bandwidth), VFO, RF power percentage,
RIT, split and TX frequency, repeater shift/offset, and CTCSS transmit tone.
Power is 0–100 percent of the radio's normalized RFPOWER range, not watts.
Rows without readback are disabled. A Set request checks fresh readback and reports a rejected or unconfirmed change. The radio/backend may reject unsupported commands. Controls are locked during TX.
PTT test explicitly keys for one second without playing audio; closing its panel
cancels/releases an outstanding test. PTT OFF / Stop TX remains available.

## Audio levels (0.20)

**Setup wizard…** guides station connection, audio devices and levels, and CAT /
Hamlib. It is available in the Option and Radio menus and directly on the Radio
tab. Choose the station role, then open the guided check in the audio step. Both computers need
1.3.3 or newer to run the shack audio wizard from a laptop while paired. Choose SignaLink USB, a radio
with built-in USB audio, another USB interface or a computer sound card, then
choose one named sound device for both RX and TX. Refresh rescans USB devices;
Advanced retains separate input/output and manual ALSA names. The choice is
applied only on Finish. SignaLink normally appears as USB AUDIO CODEC, a name
also used by other USB interfaces/radios; the wizard does not infer a radio
model from that name. Duplicate device names retain distinct card IDs.

For SignaLink, **Optimize SignaLink audio levels** unmutes/raises the selected
USB card's playback controls and sets the wizard's TX level to 100%. The normal
output target is 0 dB where supported; readback must confirm the result. This
action does not play audio, change computer-speaker/default routes, adjust RX
capture controls, connect a radio or key Hamlib. Use Play test tone separately
to check the SignaLink PTT LED. A radio is not needed for that LED check.
Finish keeps the selected card's changes and the TX level. Cancel restores
original per-channel playback values/mutes; other optimized cards are restored
on Finish. The card identity is checked before restoration, and failures are
shown in the application status. Unsupported mixers fall back to manual setup.

SignaLink's final-page PTT checkbox explicitly disables automatic Hamlib PTT
on Finish. It leaves Hamlib's connection and other radio settings intact, so
CAT control can still be used. Other connection types do not change PTT.
Cancel changes neither audio nor PTT settings. A SignaLink can key the radio
from the wizard's output tone even though the wizard never calls Hamlib PTT.

The **Audio levels** button scrolls to the Radio tab's TX/RX panel; **Back to
tuning** returns to the frequency display. These controls work without Hamlib.
TX level is 0–100% of the existing generated waveform, including Morse ID and
exported WAV files. It takes effect on the next transmission and stays locked
during preparation/playback. RX gain is −24 to +24 dB, applied to live S16_LE
capture before decoding and recording. Existing WAV files are decoded as stored.
Defaults are 100% and 0 dB, and settings persist immediately.

The separate meters display peak and RMS levels in dBFS, with a green digital
headroom guide. They do not measure RF deviation, ALC or analog microphone
levels. Source clipping is detected before RX gain; reducing software gain does
not clear that warning. Correct source overload in the interface/Ubuntu input
mixer. Excess RX gain also gets a separate clipping warning. No automatic gain,
compression or noise processing is added to the SSTV signal.

**Audio devices…** opens the existing setup wizard. Input metering is explicit,
stops after 30 seconds, and can use a computer microphone or receiver interface.
The output check plays a one-second 1900 Hz tone at the selected TX level.
Checks are mutually exclusive and stop when SSTV audio/preparation starts,
settings change devices, or the app closes. Neither requests PTT; VOX can still
key a connected transmitter in response to sound. Normal RX/TX feeds the meters
without opening additional devices. TX meter timing follows playback, excluding
PTT lead/tail delays, and includes the Morse ID.

For acoustic speaker-to-radio-microphone SSTV, use the output slider together
with speaker volume, microphone distance and the radio's microphone gain;
check the radio's own ALC/deviation. The input meter can inspect a computer
microphone, but this feature does not implement microphone voice passthrough.

## Transmission and recovery

Automatic PTT requires a connected radio. Encoding happens before keying. After
PTT ON is acknowledged, OpenSSTV waits the lead delay, plays the complete image
and optional Morse ID, waits the tail delay, then releases PTT. The countdown
includes both delays. Completion and automatic TX full-screen exit happen only
after PTT OFF succeeds. WAV export never keys the radio. To use manual PTT/VOX,
turn off automatic PTT; ordinary audio playback then remains manual.

Stop, audio failure, detected connection failure, a player exceeding its planned
duration by five seconds, and application close attempt to release owned PTT.
Pending key operations are cancelled before audio can start. The Hamlib worker
serializes operations; communication timeout/retry defaults are bounded at
500 ms/zero retries per request, although a backend may perform several requests.

If PTT OFF fails, the app reports **PTT OFF not confirmed**, leaves the TX
progress incomplete, and blocks further transmission while PTT is uncertain.
Restore the connection and use PTT OFF / Stop TX again, or disconnect/reconnect
and verify RX. A disconnected cable or unresponsive radio can prevent software
from releasing physical PTT; use the radio/interface's local controls as needed.
Forced termination or power loss also cannot guarantee PTT release.

## Implementation and validation

OpenSSTV dynamically links the system Hamlib 4 library and uses its public C
API on a dedicated worker thread. It does not access private RIG structures.
Builds prefer the distribution's `libhamlib-dev` headers; runtime-only builds
can use the small public ABI declaration subset in `src/hamlib_api.h`, checked
against Hamlib 4.6.5. The Debian package declares the Hamlib runtime and
`libhamlib-utils` dependencies. No external daemon is started by OpenSSTV.

Tests exercise native Hamlib Dummy controls and PTT, settings persistence,
full-screen/Morse/timing behavior, delayed key cancellation, failed key/release,
audio failure, connection faults, watchdog and close cleanup. These are internal
tests without RF transmission. Real radio models and serial/GPIO interfaces
still need station testing. A native rigctld/Dummy integration test is included;
this managed environment prohibits sockets, so that test is skipped here and
the network connection path remains unverified here.

References: [Hamlib public API](https://hamlib.sourceforge.net/html/rig_8h.html),
[Hamlib level and signal-strength semantics](https://hamlib.sourceforge.net/manuals/4.7/group__rig.html),
[rigctl configuration](https://hamlib.sourceforge.net/html/rigctl.1.html),
[rigctld server](https://hamlib.sourceforge.net/html/rigctld.1.html).
