# MMSSTV compatibility acceptance

Baseline: the 43 named modes in JE3HHT's MMSSTV source, including AVT 90 and
all six narrowband modes. All need both TX and RX. A mode appearing in this
catalog does not imply it has been implemented or verified.

**43 implemented; 0 pending implementations; 0 independently verified with MMSSTV.**

## Mode matrix

| Mode | Family | Frame | Implementation | MMSSTV TX/RX evidence |
| --- | --- | --- | --- | --- |
| Robot 36 | Robot | 320 × 240 | WAV TX/RX; internal tests pass | Not collected |
| Robot 72 | Robot | 320 × 240 | WAV TX/RX; internal tests pass | Not collected |
| AVT 90 | AVT | 320 × 240 | WAV TX/RX; internal tests pass | Not collected |
| Scottie 1 | Scottie | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| Scottie 2 | Scottie | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| Scottie DX | Scottie | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| Martin 1 | Martin | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| Martin 2 | Martin | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| SC2 180 | Wraase | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| SC2 120 | Wraase | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| SC2 60 | Wraase | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| PD50 | PD | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| PD90 | PD | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| PD120 | PD | 640 × 496 | WAV TX/RX; internal tests pass | Not collected |
| PD160 | PD | 512 × 400 | WAV TX/RX; internal tests pass | Not collected |
| PD180 | PD | 640 × 496 | WAV TX/RX; internal tests pass | Not collected |
| PD240 | PD | 640 × 496 | WAV TX/RX; internal tests pass | Not collected |
| PD290 | PD | 800 × 616 | WAV TX/RX; internal tests pass | Not collected |
| P3 | Pasokon | 640 × 496 | WAV TX/RX; internal tests pass | Not collected |
| P5 | Pasokon | 640 × 496 | WAV TX/RX; internal tests pass | Not collected |
| P7 | Pasokon | 640 × 496 | WAV TX/RX; internal tests pass | Not collected |
| MR73 | MR | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MR90 | MR | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MR115 | MR | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MR140 | MR | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MR175 | MR | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MP73 | MP | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MP115 | MP | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MP140 | MP | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MP175 | MP | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| ML180 | ML | 640 × 496 | WAV TX/RX; internal tests pass | Not collected |
| ML240 | ML | 640 × 496 | WAV TX/RX; internal tests pass | Not collected |
| ML280 | ML | 640 × 496 | WAV TX/RX; internal tests pass | Not collected |
| ML320 | ML | 640 × 496 | WAV TX/RX; internal tests pass | Not collected |
| Robot 24 | Robot | 160 × 120 | WAV TX/RX; internal tests pass | Not collected |
| B/W 8 | Monochrome | 160 × 120 | WAV TX/RX; internal tests pass | Not collected |
| B/W 12 | Monochrome | 160 × 120 | WAV TX/RX; internal tests pass | Not collected |
| MP73-N | Narrowband | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MP110-N | Narrowband | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MP140-N | Narrowband | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MC110-N | Narrowband | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MC140-N | Narrowband | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |
| MC180-N | Narrowband | 320 × 256 | WAV TX/RX; internal tests pass | Not collected |

## What “solid” means

Internal tests and external interoperability are separate gates. Promote a mode
to verified only after recording the software version, fixture provenance,
settings and results for both directions:

1. Decode independently produced MMSSTV audio with OpenSSTV.
2. Decode OpenSSTV audio with MMSSTV.
3. Use color bars, grayscale ramps, gradients, edge/row markers and a photograph.
   Check color mapping, component order, every row, final pixels, image geometry
   and measured transmission time.
4. Check valid/invalid headers and automatic mode selection; include recordings
   with leading silence and a manual start without a header.
5. Measure degradation with noise, tone offsets, sample-clock drift, jitter,
   clipping and interrupted audio. Establish per-mode tolerances against the
   independent baseline before declaring that robustness gate passed.
6. Test the intended sound interface on Ubuntu, including device loss, stopping,
   restarting, long receive sessions and PTT release after any error/cancel.

A clean loopback cannot establish weak-signal or radio compatibility. Quantitative
noise/clock tolerances remain acceptance work and are not established by the
in-tree implementation tests.

## Remaining validation and receiver work

- Collect independent MMSSTV TX/RX recordings for every mode. All 43 target
  implementations are enabled; implementation tests do not close this gate.
- Verify the selected legacy timing profiles and AVT header variants against
  real software and radio recordings; see SOURCES.md for exact profiles.
- Streaming RX: preserve DSP state across blocks and recover after gaps or an
  interrupted image. Live Auto now accepts consecutive complete images without
  stopping capture; previews still use batch demodulation.
- Validate the opt-in sample-clock/slant correction and leader AFC with real
  recordings, along with radio input/output and CAT/PTT on the intended hardware.

## Current automated evidence

- Complete waveform encode/decode and row geometry checks cover all target
  families, with a separate roundtrip case for every one of the 43 modes.
- Independent sample-time fixtures cover Martin 2, MC140-N, Robot 24, B/W 8/12,
  SC2-60/120 and AVT 90. They do not call the production scan scheduler or encoder.
- AVT fixtures cover both marker values, counter directions, damaged words,
  a leading attention tone, noise, clock/frequency offsets, 48 kHz audio, lost VIS,
  manual starts, truncated images, wrong markers and bad complements.
- UI checks confirm every catalog mode is present in both RX and TX selectors.
- Split-header Auto detection tests cover all 43 modes, including early AVT
  notification. Session tests check silence, invalid parity, unsupported VIS,
  invalid AVT countdown recovery and consecutive frames in different modes.
- Simulated live audio checks the detected-mode display, Auto remaining selected,
  stale manual timing, complete-picture history and Finish RX without duplicates.
- Automatic PNG checkpoints cover partial and complete images, fixed-mode sessions,
  repeated pictures, Stop/close preservation, WAV imports and full-screen receiving.

OpenSSTV 0.12 adds opt-in legacy auto clock/AFC and checksum-validated FSKID
receive. Independent synthetic fixtures exercise positive/negative mistuning,
clock errors, noisy/missing sync pulses, partial/consecutive images and split or
corrupt FSKID trailers. These remain synthetic evidence, not MMSSTV or radio
interoperability confirmation.
