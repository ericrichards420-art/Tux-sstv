# Classic test patterns

Open **Test patterns…** below the TX image, or **File → Test patterns…**. The
picker shows a large preview, historical context and the purpose of each chart.
Choose **Use pattern**, then use the normal callsign, text, fullscreen, WAV export
and TX controls. Choosing an image never starts transmission. Cancel keeps the
current picture and its text. Test patterns stay in this picker; the bottom
**Transmit pictures** strip contains only photos the user loads.

The library contains ten images:

| Pattern | ID | Purpose and background |
| --- | --- | --- |
| Original OpenSSTV chart | `test-pattern` | Existing color bars, gray ramp and checkerboard; unchanged pixels and default image. |
| SMPTE-style bars | `smpte` | Three-band American television layout: 75% main bars, reverse blue strip, color/white/dark patches. |
| EBU 75% bars | `ebu` | European full-field 100/0/75/0 bars, with full white and 75% color components. |
| PM5544-style card | `pm5544` | Original drawing inspired by the Philips circle-card tradition: color bars, gray steps, detail blocks and an alignment grid. |
| Monoscope-style card | `monoscope` | Original early monochrome-TV-style circle chart, resolution wedges, corner targets and gray steps. |
| Grayscale steps | `grayscale` | Eleven-step staircase, smooth ramp and reverse staircase for brightness/detail comparisons. |
| Crosshatch | `crosshatch` | Grid and center target for slant, geometry and alignment comparisons. |
| Multiburst | `multiburst` | Six packets of increasing spatial detail, plus horizontal stripe groups. |
| Siemens star | `siemens` | Radial focus/resolution target for comparing sharpness and aliasing. |
| Checkerboard | `checkerboard` | Black/white square edges for blur, ringing and shading comparisons. |

These are rendered RGB pictures for transmission through SSTV. They do not
produce calibrated NTSC/PAL waveforms, subcarrier phase, sync signals or
below-black levels. The SMPTE-style dark patches use visible full-range RGB
values 0, 8 and 20; they are not a calibrated broadcast PLUGE signal. Multiburst
numbers are cycles per image block, not MHz. The Philips and monoscope cards are
new geometric drawings, not archival station cards or exact reproductions.
Fine detail can be lost when scaled to a selected SSTV mode or compressed by MCA;
compare the actual TX preview with the received picture.

The original chart remains 640×496. Additional charts use a 768×576 4:3 canvas
and the existing image-fitting path for each mode. New selections clear the
previous caption and template report fields. A canceled picker changes nothing.
Choosing a built-in pattern does not add a thumbnail to the user photo library.

## Command line

```bash
build/opensstv --list-patterns
build/opensstv --pattern build/ebu.png --pattern-id ebu
build/opensstv --encode build/ebu.png --mode robot36 --output build/ebu.wav
```

Without `--pattern-id`, `--pattern` still writes the original chart. An unknown
ID fails without replacing the requested file. `--pattern-id` requires
`--pattern`.

## References

The program draws the artwork locally with Qt. No downloaded station logos,
portraits or scans are embedded. Sources used for pattern families and signal
facts:

- [Tektronix, NTSC Video Measurements](https://download.tek.com/document/25W_7247_1_rfs.pdf): color bars, multiburst and staircase applications.
- [GEKCO, Video Test Pattern Generators](https://www.gekco.com/vidprmr.htm): SMPTE three-band proportions, full-field bars, staircase, crosshatch and multiburst patterns.
- [Philips Semiconductors, Desktop Video (1994)](https://bitsavers.trailing-edge.com/components/philips/_dataBooks/1994_Philips_Desktop_Video.pdf): manufacturer diagram of EBU 100/0/75/0 bars and RGB levels.
- [Philips, TV Measuring Equipment (1980)](https://frank.pocnet.net/other/sos/Philips_TV-MeasuringEquipment_1980.pdf): PM5544 generator and combined color/monochrome circle card.
- [The Sarnoff Collection, Indian-Head Test Card](https://davidsarnoff.tcnj.edu/2017/06/02/item-of-the-week-indian-head-test-card/): monoscopes and the early TV alignment-card tradition.
- [Möller-Wedel Optical, Reticles](https://moeller-wedel-optical.com/en/product/reticles/): Siemens star as a resolution-test target.

## Checks

Automated checks cover source color levels, monochrome content, distinct catalog
images, unchanged original pixels, fitting all SSTV mode sizes, actual mouse
selection of each picker entry and loaded-photo thumbnail, Cancel preserving a report,
new selection clearing it, CLI validation, and a captured fake-audio TX waveform
decoded back to the expected MCA FM Fast preview. Tests use isolated preferences
and simulated playback; no sound device or radio is opened.
