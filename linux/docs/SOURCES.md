# Protocol sources and unresolved differences

Reviewed 2026-11-19. Sources are linked, not vendored. The implementation is new
C++ code using the documented protocol facts; it is not a Windows/VCL port.

Morse identification uses the character table and element/letter spacing in
[ITU-R M.1677-1](https://www.itu.int/rec/R-REC-M.1677-1-200910-I/en).
The 1200/WPM millisecond dot length follows the PARIS 50-unit convention in
[ARRL's Morse timing standard](https://www.arrl.org/files/file/Technology/x9004008.pdf).
The ID uses regular timing at the selected speed. The 500 ms separation after
SSTV, 200 ms trailing silence and 5 ms keying ramps are application choices.

1. **JE3HHT Makoto Mori, MMSSTV original source.**
   [SSTVModeList in sstv.cpp](https://github.com/n5ac/mmsstv/blob/master/sstv.cpp)
   defines the 43-mode compatibility target. The
   [original mode.txt](https://github.com/n5ac/mmsstv/blob/master/mode.txt),
   translated by JA7UDE Nob Oba, specifies MP, MR, ML, MP-N, MC-N and their
   identifiers/timing. The [official download page](https://hamsoft.ca/pages/mmsstv.php)
   identifies the Windows 1.13A baseline.

2. **JL Barber N7CXI, Proposal for SSTV Mode Specifications**, Dayton SSTV Forum,
   20 May 2000.
   [Paper](https://www.classicsstv.com/downloads/daytonpaper.pdf).
   Primary timing reference for Martin, Scottie, Robot 36/72, SC2-180, PD and
   Pasokon; contains the color conversion reference. Its Robot-family data came
   from Robot 1200C firmware; Pasokon and PD data came from their authors.

3. **ON4QZ Johan Maes, QSSTV source**:
   [sstvparam.cpp](https://github.com/ON4QZ/QSSTV/blob/master/src/sstv/sstvparam.cpp).
   Cross-check for mode inventory, dimensions, identifiers and timing differences.
   Do not copy the duplicate MR140/MR175 identifier in that table: Mori's original
   specification assigns **MR175 0x4C23**.

4. **dnet, pySSTV source**:
   [color.py](https://github.com/dnet/pySSTV/blob/master/pysstv/color.py) and
   [sstv.py](https://github.com/dnet/pySSTV/blob/master/pysstv/sstv.py).
   Cross-check only. There are differences in Scottie color scan duration and
   first-line sync, Martin 2 display width, SC2 timing, and color-range mapping.
   These differences are why a self-roundtrip alone cannot certify compatibility.

5. **OK2MNM Martin Bruchanov, Image Communication on Short Waves, chapter 4**:
   [Mode descriptions](https://sstv-handbook.com/download/sstv_04.pdf).
   Historical and AVT protocol context. It also demonstrates that published
   Robot and B/W timing profiles differ; the selected implementations still
   need comparison with independently recorded MMSSTV audio.

6. **MMSSTV AVT marker report**,
   [original repository issue #3](https://github.com/n5ac/mmsstv/issues/3).
   Reports the 1.13A encoder's 010 marker where the published AVT 90 marker is
   101. This is a reported compatibility difference, not a verified recording.

7. **QSSTV-Experimental maintained receiver**,
   [modeavt.cpp](https://github.com/jbdubbs/QSSTV-Experimental/blob/main/src/sstv/modes/modeavt.cpp).
   Cross-check for complemented 16-bit words and image-origin recovery from the
   five-bit count. No modem source from that project is vendored.

Specific points to preserve in reference testing:

- Scottie emits an extra initial 9 ms sync. Its regular sync is between blue and
  red, and its color scans are 138.240, 88.064 or 345.600 ms.
- Narrowband video tones are 2044–2300 Hz, with 1900 Hz sync. N-VIS is four
  six-bit symbols, MSB first: 45, 21, mode code, 21 XOR mode code. It is different
  from the wideband extended VIS format.
- Extended VIS sends all 16 bits LSB first with low byte 0x23, without a separate
  seven-bit VIS parity bit. MR/ML add 0.1 ms last-pixel holds.
- MC110-N is implemented with the original mode.txt's 143 ms color scan. Its
  calculated image duration is therefore 112.000 s, despite its name and shorter
  duration in other tables. Compare with MMSSTV before calling it compatible.
- Nominal mode names/durations are rounded. OpenSSTV displays durations calculated
  from its actual schedule. For example, PD120 with 121.600 ms components,
  20.000 ms sync and a 2.080 ms porch has 126.10304 s of image data.

No externally generated MMSSTV WAV/PNG reference pair is included yet. The
independent test generators in this repository are specification fixtures and
must not be described as recordings from MMSSTV.

## Profiles enabled in 0.2.0

| Mode | Implemented scan profile |
| --- | --- |
| Robot 24 | 160×120; 200 ms rows; 9 ms sync + 3 ms porch; Y 88 ms; two 44 ms chroma scans with 6 ms separators/porches |
| B/W 8 | 160×120; 7 ms sync + 59.9 ms full-range luminance; 8.028 s image |
| B/W 12 | 160×120; 7 ms sync + 93 ms full-range luminance; 12.000 s image |
| SC2-60 | 320×256 RGB; 78.128 ms components; 5.5006 ms sync + 0.5 ms porch |
| SC2-120 | 320×256 RGB; 156.5 ms components; 5.52248 ms sync + 0.5 ms porch |
| AVT 90 | 320×240 RGB; 125 ms components; no line sync; 32 digital words after standard VIS |

SC2 scan periods and total offsets follow JE3HHT's published `sstv.cpp` timing
parameters. Robot 24 uses the 12 ms sync/porch and 88/44/44 ms scan contract in
the handbook. The B/W 8 profile is also implemented by pySSTV; the B/W 12 profile
is described in the handbook. Other B/W timing variants exist. These choices
are explicit profiles awaiting external interoperability measurements.

AVT uses 102.4 baud (9.765625 ms per bit), one 1900 Hz start bit followed by
16 MSB-first data bits at 1600/2200 Hz. The upper byte contains the mode marker
and five-bit counter, and the lower byte is its complement. TX uses the published
101 marker and counters 0–31. RX accepts 101 and, when AVT 90 is selected by VIS
or forced, the reported 010 marker; two consistent words determine the counter
direction and image origin. The emitted VIS/control header lasts 6222.5 ms.
The implementation omits optional attention tones, and RX searches past them.
No interlaced or narrow AVT variants are claimed.


# Hamlib radio control

Radio control uses the system Hamlib 4 shared library (LGPL 2.1 or later), its
[public C API](https://hamlib.sourceforge.net/html/rig_8h.html), and the installed
`rigctl -l` model catalog. The runtime-only declaration subset was checked against
[Hamlib 4.6.5 rig.h](https://github.com/Hamlib/Hamlib/blob/4.6.5/include/hamlib/rig.h).
No private RIG structure layouts are copied or accessed. Direct/NET configuration
follows the official [rigctl](https://hamlib.sourceforge.net/html/rigctl.1.html)
and [rigctld](https://hamlib.sourceforge.net/html/rigctld.1.html) manuals.


## FSKID receive framing

Primary protocol reference: ON4QZ's [QSSTV receiver](https://github.com/ON4QZ/QSSTV/blob/main/src/sstv/visfskid.cpp), inspected for framing facts (six LSB-first bits per symbol, 22 ms bits, 1900/2100 Hz, marker 0x2A, terminator 0x01, XOR and modified ASCII). OpenSSTV implements its own bounded waveform detector and independent synthetic test fixtures; it does not embed QSSTV code. MMSSTV's author documents FSKID receive support in the [engine manual](https://hamsoft.ca/pages/programmers/mmsstv-engine.php).

## Classic pattern artwork

See [TEST_PATTERNS.md](TEST_PATTERNS.md) for manufacturer and museum references.
All additional pattern artwork is drawn locally in Qt; source scans, station
logos and portraits are not redistributed. These images adapt familiar pattern
families for SSTV rather than synthesize calibrated broadcast-video test signals.

## SSTV audio levels

The author's [MMSSTV SSTV primer](https://hamsoft.ca/pages/mmsstv/sstv-primer.php)
describes sound-card input levels and microphone/output/ALC setup. Device access
continues through ALSA-utils arecord/aplay, using documented raw signed-16 PCM
and WAV interfaces ([ALSA PCM documentation](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm.html),
[ALSA-utils source](https://github.com/alsa-project/alsa-utils/blob/master/aplay/aplay.c)).
OpenSSTV's gain and dBFS meter implementation is original; digital meters do not
claim to measure the radio's analog/RF operating level.

## USB audio setup

[Tigertronics communication-program setup](https://tigertronics.com/pgmsetup.htm),
[SignaLink troubleshooting](https://tigertronics.com/slusbts.htm) and the
[SignaLink USB FAQ](https://tigertronics.com/slusbfaq.htm) identify USB Audio CODEC
as SignaLink's sound device and distinguish its audio-operated PTT from CAT.
The wizard's SignaLink profile uses those distinctions without assuming all
USB Audio CODEC devices are SignaLinks. Radio-specific USB menu configuration
still depends on the radio's own documentation.

The [ALSA PCM plugins documentation](https://www.alsa-project.org/alsa-doc/alsa-lib/pcm_plugins.html)
defines the plug layer's automatic format, rate and channel conversion. The
combined selector prefers `plughw:CARD=…,DEV=…` endpoints so USB audio hardware
can accept the application's mono 12000 Hz receive and 48000 Hz test output
formats through conversion. Raw or custom PCMs remain available in Advanced.

The [ALSA amixer implementation](https://github.com/alsa-project/alsa-utils/blob/master/amixer/amixer.c)
and the installed amixer manual define explicit-card selection, playback-only
simple mixer commands, dB/percentage volume setting, unmute and raw control
read/write. Playback optimization follows Tigertronics' guidance above to avoid
attenuating the SignaLink's computer playback path and to use the interface's
TX knob for radio drive. OpenSSTV verifies mixer readback, not the physical PTT
indicator or RF output.


## TS-990S mode names and USB/data PTT

Kenwood's [PC command reference](https://www.kenwood.com/i/products/info/amateur/pdf/ts990_pc_command_en_rev2.pdf)
defines OM data profiles, CB/TB band selection and TX0/TX1 input routing.
The [USB Audio manual](https://kasc.kenwood.com/files/prod/2452/5/TS-990S_USB_Audio_manual.pdf)
and [USB settings guide](https://www.kenwood.com/i/products/info/amateur/ts_990/pdf/ts990_ft8_settings_en.pdf)
document radio-side USB and DATA SEND setup. The separate laptop mic, Wi-Fi and
shack PCM path is Tux-sstv's implementation.

The public [Hamlib 4.6.5 mode enums](https://sources.debian.org/src/hamlib/4.6.5-5/include/hamlib/rig.h/)
identify USB/LSB D1–D3 values. A small original name mapping covers releases
whose generic mode-name parser/formatter omit these public enum names. The
[Kenwood backend](https://sources.debian.org/src/hamlib/4.6.5-5/rigs/kenwood/kenwood.c/)
provides CAT behavior through the public API. Independent Python pseudo-terminal
fixtures emulate only exercised commands, never real RF or serial hardware.


## Complete emoji catalog (1.12.0)

The catalog is the [Unicode Emoji 17.0 RGI set](https://www.unicode.org/Public/17.0.0/emoji/emoji-test.txt):
3,944 fully qualified emoji plus nine components. `tools/update_emoji_catalog.py`
extracts this set from installed ICU 78.2 using the public
[UnicodeSet API](https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/uset_8h.html).
English names and search annotations come from Unicode CLDR through IBus's
installed English dictionary. The generator derives twenty newer gender/tone
names from their CLDR base names and assigns categories to newer entries not
yet in IBus's category table. No IBus implementation code is copied.

Unicode data and annotations use [Unicode License v3](https://www.unicode.org/license.txt).
The notice is retained at `resources/licenses/UNICODE.txt` and installed under
`share/doc/tux-sstv/licenses/`. Emoji rendering uses the distribution's
[Noto Color Emoji](https://github.com/googlefonts/noto-emoji), an OFL font;
the font artwork is not copied into the source or package. Noto 2.051 adds
Unicode 17.0 support. The project does not redistribute Apple's emoji artwork.
