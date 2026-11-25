# Operate your Tux-sstv shack from another room

Tux-sstv 1.4 connects two copies of the application over your home Wi-Fi or wired LAN.
The shack PC keeps the USB sound interface and radio connection. The laptop
uses its normal picture editor, RX display, TX button and Radio console.

Install the same current Tux-sstv version on both computers. Leave the shack PC
awake and Tux-sstv running while operating from the laptop.

Tux-sstv was previously called OpenSSTV. Version requirements 1.2–1.3 below refer
to compatible pre-rename releases. The authenticated wire protocol and saved
connection key stay unchanged. Use matching Tux-sstv 1.13.1 or newer versions on
both computers for the current PTT cancellation fixes.

The one-second **PTT test** in Advanced controls now releases only the shack
activation that it started. A delayed test timeout or dialog close cannot stop
a newer activation, even when its newer status has not reached the laptop.
Closing the test dialog before its key acknowledgment defers that owned release
until the acknowledgment arrives. **PTT OFF / Stop TX** and the main **Stop**
control remain unconditional emergency actions. A newer laptop disables timed
PTT tests when an older shack does not advertise ownership support; update both
computers to use them. An older laptop cannot gain the new ownership protection
solely by updating the shack.

## On the shack PC

1. Set up the SignaLink USB or radio's built-in USB audio using **Option → Audio
   setup wizard**. Confirm local transmit and receive first.
2. If you use CAT, configure the radio under **Option → Settings → Radio** and
   connect it. SignaLink audio-operated PTT works without a CAT connection.
3. Open **Remote → Remote station…**, or the **Remote station…** button on the
   Radio tab. Choose **Share this shack PC** and save.
4. Open the dialog again to read the shack address, connection port and key.
   Copy those three values to the laptop. Keep the key private.

## On the living-room laptop

1. Open **Remote → Remote station…**.
2. Choose **Connect to shack PC**. Enter the shack address, the same port
   (normally **48765**) and the connection key copied from the shack PC.
3. Save. The Radio tab reports the connection and incoming shack audio. Reception
   starts automatically; the laptop microphone is not used for SSTV.
4. Prepare a picture normally and press **Play TX**. The laptop sends the complete
   waveform to the shack first. The shack then plays it through its selected USB
   sound interface and handles PTT locally. Laptop speakers are not used for TX.
5. The Radio console uses the shack's Hamlib connection for tuning, mode, VFO,
   memory/VFO and other controls supported by that radio. If CAT is configured
   but not yet connected, **Radio → Connect** connects the shack radio.

During playback, the shack automatically switches to its RX picture page and
shows **Remote TX monitor**. The image builds as the outgoing SSTV waveform is
played, and the waterfall follows that audio. It uses the transmitted waveform,
including the laptop's text and picture edits. Manual RX mode, clock and tone
corrections do not affect this clean local monitor. Stop or a connection loss
keeps the partial preview; the next transmission starts with a fresh display.
The normal receiver resumes automatically after TX.

The monitor is labeled as outgoing audio and is not automatically added to the
receive history. **Save copy** can save its displayed image. It does not confirm
RF output or that another station received the picture.

Use **Stop / PTT OFF** on either computer to stop transmission. Only one laptop
can connect to a shack at a time. Either computer can transmit and use radio
controls when the station is idle. The first transmission reserves the radio;
the other computer waits until it finishes or is stopped. Shared USB audio
settings stay on the shack. Use the audio step of the Setup wizard to change them while
paired and idle. Hamlib setup can also be changed from either computer.
Changing connection roles preserves each computer's local audio selections and
radio configuration.

## Setup access and local USB operation

From 1.3.4, Settings remains accessible on a paired shack. Hardware edits use
its dedicated audio/Hamlib setup pages; the station is reserved while General
settings is open. Setup entries remain clickable during TX and explain the
blocking operation. **Stop and open setup** waits for stop/PTT-off completion
before opening the requested page; Cancel keeps the current transmission.

The Settings banner identifies local, shared-shack and remote-laptop operation.
For a TS-990S or another radio attached to this computer, choose **Set up this
computer's USB radio/audio…**. Offline remote radio/audio dialogs offer local
setup directly too. This changes the operating role to Local USB without
removing the saved remote address, key or profiles. Reopen Remote station to
return to Wi-Fi operation. If the radio is attached to the shack, keep remote
mode and use its setup pages.

## Setup wizard while connected

Use **Tux-sstv 1.5.0** for the unified Setup wizard. Its remote audio check also
works with a shack running OpenSSTV 1.3.3 or newer. Open **Option → Setup
wizard…**, **Radio → Setup wizard…**, or the visible wizard button on the Radio
tab. Choose the station role and remote connection, then open the guided audio
check in the audio step. CAT/Hamlib follows in its own wizard page. The paired shack can open its normal wizard too; it is no longer
hidden or redirected to Wi-Fi pairing.

From the laptop, the guided audio check is titled **Shack PC over Wi-Fi**. Device discovery,
input metering, output test tones and USB playback-level optimization all run
on the shack. Select SignaLink USB or the radio's built-in USB sound card there,
then work through the same four guided pages. The laptop displays live controls
and readings; its own microphone and speakers remain untouched by these checks.
Voice PTT still has its separate laptop microphone selector.

The guided audio check reserves the shared station. SSTV, voice and Hamlib setup changes
wait until it ends. Receive monitoring pauses so the wizard can open the selected
input, and resumes automatically after Finish or Cancel. No tone plays without
you pressing Play test tone; a SignaLink or VOX radio can respond to that audio.

Finish saves the selected input/output devices, TX level and SignaLink PTT choice
on the shack. The laptop's local device settings are preserved. Cancel, Stop,
closing the wizard or losing the laptop connection stops checks and restores
provisional mixer changes using the existing USB-card identity checks. Restore
errors are reported rather than treated as success. Pairing stays connected
when setup finishes normally; older or disconnected hosts show an update hint.

## Hamlib setup while connected

Use **OpenSSTV 1.3.2 or newer on both computers**. The Radio tab's **Radio setup…**
button opens Hamlib setup, as does **Radio → Hamlib setup…**. It no longer opens
the remote pairing dialog on the laptop. Option → Settings → Radio (Hamlib) also
provides a button for the shack's setup when using a remote laptop.

On the laptop, the page shows the shack's enabled state, radio model, CAT device,
baud rate, rigctld connection and PTT method/timing. Configure the device path
as it exists on the shack, not on the laptop. Save updates the shack's settings
and closes its previous CAT connection; pairing stays connected. Choose **Radio
→ Connect** to open the new connection. The laptop's local radio settings are
preserved for local operation. The shack's own Radio setup remains accessible
while a laptop is paired.

Save is blocked during SSTV or voice TX, active uploads and uncertain PTT. Changed
or stale settings are rejected, so concurrent edits cannot overwrite a newer
setup silently. Saving does not key PTT or connect automatically. On a laptop,
use Save then Radio → Connect instead of the local USB connection-check button.
A disconnected or older shack shows an update/reconnect hint instead of the
laptop's unrelated radio settings.

## Send from the shack PC to the laptop

Install OpenSSTV **1.2.0 or newer on both computers**. Leave the shack sharing and
the laptop connected as above; there is no need to swap roles or move USB cables.

Prepare a picture on the shack and press **Play TX**. The radio transmits over
the air using the shack's normal audio and PTT settings. The laptop automatically
switches to RX and decodes that same played audio over the network, showing
**Shack TX monitor** with a live waterfall and a progressively built picture.
This outgoing SSTV monitor stays silent. Optional **Listen to shack** playback
resumes when the shack returns to receive. Your laptop's antenna RX mode and
correction settings remain unchanged.

Afterward, either computer can send the next picture. Stop from either side
stops the radio transmission and keeps the partial monitor image. Network loss
retains the existing stop/PTT-off behavior. Monitors show outgoing audio and do
not prove that a separate radio received the over-air signal.

## Radio profiles on the shack

With **Tux-sstv 1.13.1 on both computers**, the Radio tab profile selector and
Profiles… manager show the shack's saved radios. Switch or create profiles from
the laptop, then use Setup active radio to configure the target sound interface
and CAT connection. Saving updates the shack's active profile. Use Radio → Connect
after switching; the profile system never automatically keys or connects CAT.

The selected shack profile supplies the laptop's SSTV TX percentage. Its TX
level display follows the shack and is read-only; change it through the shack's
audio wizard or on the shack PC. Laptop microphone/speaker choices, its own USB
radio profiles and its independent receive-monitor adjustments remain local.
Old shacks advertise no profiles; the manager explains the required update.

Stale profile edits and CAT dialogs opened for a different profile are rejected.
Radio controls, voice PTT and SSTV uploads identify the intended profile too;
delayed Wi-Fi commands are rejected if another radio has been selected.
Stop/PTT OFF stays available regardless of profile identity.
If the shack changes radios while the laptop is preparing audio, that pending
transmission is cancelled. Metadata-only actions such as Rename preserve listening
and reception. See [profile setup](RADIO.md#named-radio-profiles-1130).

## Voice PTT

Install **OpenSSTV 1.3.1 or newer on both computers**, restart and reconnect using
address, port and key as before. Use the Radio tab on the laptop:

1. Choose **This computer's microphone**. **Find audio devices** lists local ALSA
   inputs and outputs; `default` uses Ubuntu's default input. Select your laptop microphone
   or headset here, keeping the shack's RX device and USB output unchanged.
2. Start with **Mic gain 100%**. Click **PTT · Click to transmit**, wait for
   **Talk now**, then speak. The button stays red and locked on after you release
   the mouse. The mic meter should move. Gain and a digital limiter keep transmitted
   PCM within headroom; reduce Ubuntu's input level if capture itself clips.
3. Click PTT again to stop and return to receive. Space also toggles PTT when
   the button has keyboard focus. Switching tabs or windows keeps the lock on.
   Closing the app or pressing Stop / PTT OFF stops transmission and clears it.

The shack streams your speech into its configured radio audio output, with a
small jitter buffer. Hamlib automatic PTT keys before speech plays. If using a
SignaLink without CAT PTT, speech triggers its audio-operated PTT; its DLY control
sets how long it stays keyed between words and after stopping. Set microphone
gain, the SignaLink TX control, radio input level and radio mode appropriately
for voice through that input. No mode change is made automatically.

Voice and SSTV reserve the same transmitter. Either PC can stop voice. The same
PTT button can use a microphone directly on the shack or in local operation.
Microphones open only while voice PTT is locked on. The remote SSTV receiver
remains automatic.

For the TS-990S USB input, install **Tux-sstv 1.11.0 on both computers**,
choose **Use USB/data PTT** in the shack's Radio setup, save and reconnect.
Select USB-D1 or the appropriate LSB/data profile and enable USB Audio for the
radio's DATA SEND source. These are separate mode and input-routing settings;
see [TS-990S setup](RADIO.md#ts-990s-usbdata-audio-and-voice-1110).
The remote controls now use the host's model, including exact data modes and
Main/Sub choices. The shack verifies control changes before acknowledging them.

### Check microphone levels without transmitting

With **Tux-sstv 1.10.0**, choose **Mic check…** in the laptop Radio tab. The check
opens only the local microphone and uses no PTT, radio output or voice network
stream. Speak normally; green voice peaks from −18 to −6 dBFS indicate useful
headroom. The original input meter reveals clipping before software gain and
limiting. Lower Ubuntu's input volume if the microphone itself clips; adjust
Mic gain for quiet or high voice peaks.

Use settings applies the selected microphone and gain for voice PTT. Cancel
leaves those choices unchanged. Stop/Start and Reset readings support repeated
checks. The laptop speakers mute for the check while remote SSTV reception keeps
running, then resume if listening was enabled. Checking works locally without
a shack connection as well. It never starts a voice transmission.

### Listen on laptop speakers

Use **Tux-sstv 1.9.0** on the laptop. In its Radio tab, choose **Laptop speakers**,
then check **Listen to shack**. `default` uses the default local output; **Find
audio devices** lists local speakers and headsets. You can also type an ALSA
output name and press Enter. The percentage beside the speaker picker controls
only listening volume. Ubuntu's output volume also needs to be audible.

The checkbox, speaker and volume are saved. Received shack audio plays while
SSTV reception continues, so incoming SSTV tones will also be audible. Uncheck
the box for silent decoding. No additional radio connection or PTT is started
by listening. The laptop microphone stays closed until you click Voice PTT.

Speaker playback mutes while either computer transmits or configures the shack,
then resumes on receive. Choosing **Radio → Disconnect** also stops speaker
playback immediately. For a CAT-enabled station, it stays stopped while the
radio is disconnected, even if the remote-station connection is still online.
Reconnect the radio from either computer to resume the saved listening choice.
If the disconnect request fails, listening is turned off and the error is shown;
it will not silently restart. Audio-only stations such as SignaLink without CAT
continue to support listening. A connection loss closes speaker playback and stops
voice TX; a reconnect can resume listening but never resumes transmission.
A speaker error turns listening off and reports the error, while SSTV RX and
microphone PTT remain available. Choose a working speaker and check the box again.
Keep the shack USB input/output assigned to the radio; the speaker picker selects
only the laptop's local output.

Broken microphone or output processes, missing voice packets, loss of the
connection and a three-minute TX limit stop voice. A voice stream that stops
arriving is cut off within about two seconds even if heartbeat messages still
arrive; the initial microphone/output setup allows five seconds. A transmission
cancelled before the PTT acknowledgment cannot start later. A failed CAT release remains
visible and blocks further TX until PTT OFF succeeds. Reconnection never resumes
a voice transmission; click PTT again deliberately. Older shack versions leave
the laptop voice button disabled and show an update hint.

## Wi-Fi interruptions

The shack plays a complete local audio file, so normal Wi-Fi timing variation
does not stretch or interrupt individual SSTV tones. If the control connection
is lost or its heartbeat expires, the shack stops playback and requests PTT OFF.
Heartbeat expiry takes up to about 6.5 seconds. A SignaLink's PTT then drops
according to its hardware delay setting. A radio connection failure can prevent
confirmation of CAT PTT release; OpenSSTV displays that error on the shack PC.

With **Reconnect automatically when Wi-Fi returns** enabled, the laptop resumes
receiving after reconnection. An interrupted transmission is never restarted
automatically. Prepare or resend it deliberately after checking the station.

## Connection troubleshooting

- Both computers must be on a LAN that allows them to reach each other. A guest
  Wi-Fi network may isolate clients.
- If the shack has a firewall, allow incoming TCP on the chosen port from the
  laptop. Do not disable the firewall or forward this port through your router.
- If the shack address changes, update it on the laptop. A router DHCP reservation
  can keep the address stable.
- If the link connects but there is no waterfall audio, check the shack's selected
  USB input, radio receive audio and interface receive level. The remote status
  reports shack audio errors separately from the network connection.
- If another laptop is connected, disconnect it first. If a key was changed,
  copy the new key to the laptop and save again.
- Hardware audio setup is performed on the shack PC. Laptop TX level changes
  affect the waveform sent to the shack; laptop RX gain applies to the incoming
  audio digitally and never changes the laptop microphone volume.

This feature carries OpenSSTV audio and radio controls; it does not install a
system-wide virtual USB device. Messages are authenticated with the connection
key and checked for alteration/replay. Audio and pictures are not encrypted.
Use the feature on your trusted home LAN, not as an Internet-facing service.
