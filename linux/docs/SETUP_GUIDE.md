# Guided station setup

Open **Option → Setup wizard**. Work through station location, audio, radio
control and transmit keying, then the final check page.

## 1. Where is the radio?

Choose this computer, a shack PC sharing its radio, or a remote laptop.
The remote laptop configures the shack's audio and radio controls while keeping
its own microphone, speakers, and local radio profile. Use matching updated
versions on both computers for the new DAX profile and guided controls.

## 2. Choose the audio connection

- **SignaLink USB:** select the USB AUDIO CODEC interface for receive/transmit.
  Check receive levels and the interface's transmit level. Its audio-operated
  PTT can key the radio when you explicitly play the test tone.
- **Radio with built-in USB audio:** choose the radio's sound device. Audio and
  CAT control can use the same USB cable or separate connections.
- **FlexRadio / SmartSDR DAX:** select the DAX RX channel assigned to your
  operating slice and the DAX TX output. The receive/transmit selectors are
  shown separately. One unambiguous DAX endpoint may be suggested; multiple RX
  channels require you to choose the intended slice. Enable that RX channel
  and DAX TX in SmartSDR. DAX audio alone does not request transmit.
- **Other devices:** choose a combined sound card or use separate input/output
  choices. Device names can also be entered manually.

The audio test checks audio. It does not send CAT PTT. Keep a working device
selection rather than replacing it with the computer's default speakers.

## 3. Choose radio control and transmit keying

Choose a **Connection setup** preset, then fill in the required fields:

| Setup | What to enter | Initial keying choice |
| --- | --- | --- |
| Radio USB or serial CAT cable | Actual radio model and CAT port | Automatic CAT command |
| SignaLink USB with a separate CAT cable | Actual radio model and CAT port | Audio-operated SignaLink/VOX |
| FlexRadio: SmartSDR CAT virtual COM port | The CAT COM port created for this application and slice | Automatic CAT command |
| FlexRadio: direct network / SmartSDR slice | The SmartSDR slice and radio address, such as `192.168.1.50:4992` | Automatic CAT command |
| Existing Hamlib rigctld server | The running server's host and port, usually 4532 | PTT through that server |
| Audio only / receive only / manual PTT | No computer CAT connection | External or manual keying |

**Start transmission with** is always a separate choice. Select CAT commands,
serial RTS, serial DTR, audio-operated keying, or manual keying to match your
station. Choosing a CAT cable no longer implies that automatic PTT should be
disabled. Opening setup preserves existing custom settings; selecting a preset
explicitly applies its displayed defaults.

For SmartSDR CAT virtual COM, Tux-sstv selects the documented TS-2000 Kenwood
compatibility mode. This is a CAT protocol choice; the physical radio remains
a Flex. For RTS/DTR instead of CAT commands, create a separate PTT port in
SmartSDR CAT and enter that port in Tux-sstv. The CAT port and dedicated PTT
port must be different in this setup.

The direct network option uses Hamlib's native SmartSDR slice driver. A radio's
native API on port 4992 and a Hamlib rigctld server on port 4532 use different
protocols. Enter each address in its matching setup, not interchangeably.

**Advanced radio and PTT settings** retains manual driver configuration,
mic/data transmit input, target VFO, timing, GPIO, and extra Hamlib options.
Kenwood TS-990S users also have a USB/DATA transmit-input shortcut.

## 4. Review and check

The review lists the saved receive/transmit devices and the actual PTT choice.
When computer radio control is disabled, it reports external/manual keying.
Saving settings does not prove that the radio or its PTT is working.

1. Choose **Connect and check radio** to open the saved connection without
   requesting transmit. Verify the radio connection and displayed state.
2. If you use automatic computer PTT, choose **Test PTT for 1 second** when
   you are ready for the radio to transmit. The button becomes available after
   connection. Confirm the radio enters TX and returns to RX.
3. Leaving the review page or closing the wizard stops its own active PTT test.
   Tests and stop actions use transmission ownership so an old test cannot
   release a later transmission started elsewhere.
4. Finish and check an actual SSTV transmission with the intended audio path.

A successful command acknowledgement does not measure RF output or confirm
audio routing. Native Flex/SmartSDR operation still depends on the installed
Hamlib driver, selected slice, and SmartSDR configuration. An upstream
[SmartSDR PTT report](https://github.com/Hamlib/Hamlib/issues/2267) describes
intermittent slice-ownership errors in Hamlib 4.7.2; the virtual COM option
uses the separate CAT compatibility route.

## Reference material

- [FlexRadio SmartSDR CAT User Guide](https://edge.flexradio.com/www/offload/20260331200739/SmartSDR-CAT-User-Guide.pdf)
- [FlexRadio DAX and PTT setup guidance](https://helpdesk.flexradio.com/hc/en-us/articles/49847922262427-How-to-Configure-HRD-using-DAXv2)
- [Hamlib supported radio models](https://github.com/Hamlib/Hamlib/wiki/Supported-Radios)

Original developer: **Eric Richards, KI5BZO**.
