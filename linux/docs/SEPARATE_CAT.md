# Separate audio interface and CAT cable

Tux-sstv can use a SignaLink USB or another audio interface at the same time
as a separate CAT cable. The sound device carries receive/transmit audio.
The CAT cable supplies frequency, mode and other supported radio controls.
The two connections keep their own device settings in the radio profile.

1. Open **Option > Setup wizard** and choose the computer connected to the radio.
2. In the audio step, keep **SignaLink USB** selected as the audio interface
   and choose its **USB Audio CODEC** sound device. Finish the audio check.
   If audio already works, continue without changing the saved sound devices.
3. In the CAT step, choose **SignaLink USB with a separate CAT cable** under **Connection setup**.
   This enables CAT radio control and leaves transmit keying to SignaLink
   or the audio interface. It keeps the selected radio model, port and baud.
4. Choose the correct **Radio model**, select the cable's **CAT port / device**
   (for example, COM7 on Windows) and match **CAT baud rate** to the radio.
   **Refresh ports** reloads local serial ports without replacing the selected
   port. When configuring a remote shack, reopen the CAT step to reload the
   shack's current port list.
5. Save with **Next**, review both audio devices and the CAT port, then Finish.
   Use **Radio > Connect** to open CAT. This keeps the audio devices selected.

The explicit SignaLink connection preset chooses the audio-interface/manual PTT
method and turns off automatic CAT PTT. CAT frequency and mode controls stay
enabled. It does not play audio, connect automatically or request PTT.
Use the normal sound-interface transmit test to check audio-operated keying.
For built-in USB audio or another interface that needs computer PTT, choose the **Radio USB or serial CAT cable** preset, then select **Start transmission with**. See [the guided setup instructions](SETUP_GUIDE.md) for FlexRadio, separate RTS/DTR PTT ports, and connection checks.

## Example: Yaesu FT-857D

Connect SignaLink to the radio's DATA connection and the separate CAT cable
to CAT/LINEAR. Select **Yaesu FT-857** in Hamlib (model 1022). On the radio,
Menu 020 **CAT/LIN/TUN** selects **CAT**; Menu 019 **CAT RATE** sets the speed,
which must match the program's CAT baud setting. Its listed rates are 4800,
9600 and 38400 baud. Keep the audio/mode configuration that already works
with SignaLink. Other radios use their own model and CAT-port settings.

Sources: [Tigertronics SignaLink FAQ](https://tigertronics.com/slusbfaq.htm),
[Yaesu FT-857D operating manual, Menu 019/020](https://www.yaesu.com/Files/4CB893D7-1018-01AF-FA97E9E9AD48B50C/FT-857D_OM_ENG_EH007M108.pdf).
