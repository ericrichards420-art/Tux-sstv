# Architecture

## Built-in picture collection (1.13.1)

`resources/tux_pictures.qrc` embeds the 14 full-size PNGs listed in
`resources/pictures/tux/catalog.json`. `MainWindow::loadBuiltInPictures` caches small
preview images once per process and populates a separate Tux gallery. Its
selection handler reads the unchanged original into `source_`, using the same
composition path as personal photos. Mode changes therefore compose from the
full original, independently of the gallery thumbnail or previous output size.
The same loader populates a separate Holidays gallery from the holidays resource prefix and catalog. User photo settings and receive-history files remain separate. See ARTWORK.md
for dimensions, provenance and verification hashes.

## Radio and modem

`core/morse_receiver` receives 12000 Hz mono CW on the existing receive worker.
A 256-sample Hann window with 512-point FFT advances every 60 samples (5 ms).
Coherent narrow tone selection and competing-tone rejection precede mark/gap
measurements. Fit dot/dash and intra-character/character gaps across the word;
commit on a word gap, or finish a recording after a final character gap in
core tests. Only syntactically valid complete words become callsign candidates.
The transcript is bounded to 160 characters and in-flight runs to 180.

Receiver tracks discarded sample positions and excludes known SSTV spans
before passing each new PCM block to Morse. Excluding just the picture span
preserves the first CW mark when it shares a block with the image end. A new
capture creates fresh timing state. JobResult carries an immutable reading
to the UI; the GUI never accesses the active decoder. Its read-only text
remains copyable, while Use callsign explicitly confirms a candidate for the
existing Reply/contact path. PNG CallsignSource distinguishes confirmed Morse
from FSKID, including after selecting an image from history. No automatic
Morse result overwrites FSKID. This version integrates live PCM only.

Automatic local/shack capture tracks time since nonempty raw PCM, independent
of audio amplitude. The one-second receive timer detects an alive capture
process delivering no PCM for eight seconds, stops its capture generation,
cancels any pending decode and retries through the existing backoff path.
New capture gets a fresh receiver; remote PCM resumes on the same connection.
Valid silent PCM refreshes the timer. Intentional receive suspension and
remote client capture do not run this local helper watchdog.

In 1.13.5, idle VIS acquisition also accepts a validated remaining portion of
the opening leader. Standard and extended VIS require a run of consistent
1900 Hz measurements before the complete mode word. N-VIS retains its
frequency break, remaining leader and complete prefix/XOR validation. This
handles late input opening and an initially muted signal, including before
the next image. Full-preamble validation remains mandatory for replacing an
active image. Missing or invalid mode bits are not inferred. All 43 modes
have late-open and muted-leader tests; local and remote tests feed successive
clipped-header images through the normal automatic receive path. AFC samples
the recent remaining leader (accounting for the N-VIS frequency break),
requires robust agreement and retains the manual correction if measurement
fails. Independent waveforms verify positive and negative offsets after
clipped acquisition, including with leading muted audio.

In 1.13.4, VIS control tones and data symbols use a majority of five short
interior measurements instead of a single center measurement. This tolerates
tested 2–4 ms audio dropouts in standard, extended and narrow mode headers.
The shorter break between leaders uses a narrower measurement window. Parity,
N-VIS checksum, supported-mode, leader, stop and interruption-preamble checks
remain required. Invalid words are not repaired by guessing a mode.

`src/radio.cpp` owns Hamlib's public C API behind an injectable backend. A single
worker thread serializes connection, polling, control and PTT calls. The GUI
controls an acknowledged key → lead → audio/Morse → tail → release sequence;
its cancellation flag prevents delayed key replies from starting audio after
Stop. An uncertain PTT release blocks subsequent TX until recovery. See
[radio setup and behavior](RADIO.md) and the native Dummy/fake-backend tests.

`src/core` contains the protocol and modem code. It depends on Qt Core and Gui for
file handling and images, and can run without
the Widgets interface.

- `modes.cpp`: the complete MMSSTV target catalog and per-line tone/component
  schedules. Protocol timing is expressed in milliseconds, not integer sample
  counts. A mode includes a stable ID, frame geometry, VIS identifier and an
  explicit implementation flag.
- `codec.cpp`: aspect-preserving image fitting, color conversion, phase-continuous
  synthesis, demodulation, header recognition, line tracking and frame assembly.
- `receiver.cpp`: automatic live receive sessions, partial-frame updates, frame
  completion and rearming for the next supported header.
- `morse.cpp`: optional callsign identification using International Morse timing,
  fractional sample boundaries and 5 ms raised-cosine keying edges.
- `composition.cpp`: aspect-fitting, the transparent station callsign overlay, and
  Unicode caption rasterization. QTextDocument lays out plain text and color emoji;
  an alpha-mask outline preserves the glyph colors. Composition is converted to
  grayscale after all overlays for B/W modes.
- `wav.cpp`: checked RIFF/PCM input, explicit channel selection, and atomic WAV
  output through QSaveFile.

TX accumulates segment boundaries in fractional sample time and rounds the
cumulative position. Rounding each pixel separately would accumulate an error
large enough to slant an image. Oscillator phase continues across every segment.
Y/Cr/Cb modes use the studio-range BT.601 mapping documented by N7CXI; MP and PD
average chroma between two rows. MR/ML hold each component's last pixel for 0.1 ms.

RX resamples to 12000 Hz using a windowed sinc filter with a phase table. A complex
mixer centered at 1900 Hz and a 49-tap low-pass FIR create a baseband analytic
signal. Adjacent-sample phase differences recover frequency; the filter's delay
is compensated in the output timeline. Header bit decisions use median frequency
around symbol centers. Standard parity, the extended VIS marker, and narrowband
prefix/checksum are checked before a mode is selected.

Frame assembly uses nominal timing plus manual or opt-in measured clock/frequency correction. A validated VIS leader supplies tone offset, and a robust fit of at least 12 line-sync centers spanning three seconds supplies image origin and sample-clock scale. Insufficient evidence retains the manual values. Live previews retain frame-specific AFC and consume the corrected frame boundary.
Nearby sync runs can adjust each line's position. Scottie's initial sync and
mid-line sync location are represented separately. Color-difference line pairs
are reconstructed together. Monochrome modes use full-range luminance and show
grayscale TX previews. AVT 90 has a dedicated complemented-word countdown parser
that derives the first picture sample from two consistent counters; its RGB
image is then assembled without line sync tracking. This is a first modem implementation, not a finished
weak-signal receiver: automatic clock/AFC behavior still needs independent radio recordings, and multipath treatment requires further DSP work.

`sstvui` is a Qt Widgets layer. `mainwindow_ui.cpp` builds the classic tabbed
operating layout, quick mode grids, settings dialog and thumbnail gallery.
`composer.cpp` supplies a draft editor with a live mode-sized preview, caption
styles, nine anchors and a searchable emoji picker. Picker selections insert Unicode
at the document cursor; skin tones are added after the base character and recent
sequences are saved intact. Apply updates the current image draft; Cancel never changes the TX
composition. `emojis.cpp` holds the built-in searchable catalog. Noto Color Emoji is
an explicit Debian dependency. The GUI composes one final QImage before encoding,
so TX, full screen and the editor use the same pixels, independently of UI skin.
`skins.cpp` defines six built-in palettes and their widget styles. Settings previews
apply immediately and Cancel restores the previous skin; Save persists `ui/skin`.
Unknown IDs fall back to Classic. The spectrum uses each skin's trace and grid
colors, and save errors retain a distinct readable color across skin changes.
Image pixels and the neutral black full-screen canvas do not change with a skin.
The RX full-screen view is a nonmodal QWidget window sharing received-image updates.
It stays visible across completed frames and idle gaps; only its Escape shortcut
hides it during normal operation. Saving a frame does not dismiss or recreate the
viewer. Escape leaves the operating window and capture state unchanged.
TX has its own full-screen image window. The existing progress panel moves to its
bottom while open, keeping the same playback state and timing display. The bar
includes a whole-second countdown, rounded upward from remaining audio duration,
including Morse ID. Countdown zero can precede the player exit; the bar remains
below 100% and indicates finishing until successful playback completion. Hiding the
window restores that panel to TX; Escape never cancels the audio job or playback.
Successful playback completion also hides TX full screen, returning to the
unchanged operating window with progress at 100%. RX stays full screen until Esc.
When enabled, Morse ID is appended to the encoded image audio in the same WAV and
playback process. Its duration includes 500 ms of leading silence and 200 ms after
the final character. Tone marks use 1/3 dot units, element gaps one unit and letter
gaps three units; a unit is 1200/WPM milliseconds. The station callsign is normalized
to uppercase and validated before TX. Total progress includes the ID, switches its
caption during the Morse portion, and completes only when playback exits.
`imageview.cpp` renders the actual image fitted to the current viewer size.
`mainwindow.cpp` connects those controls to the modem and audio lifecycle. Modem jobs run on Qt Concurrent workers with an
atomic cancellation token; GUI updates return through the event loop. The current
audio adapter uses `arecord` and `aplay` as child processes with explicit argument
lists. It never constructs shell commands. Capture stays bounded to ten minutes.
Every second the UI submits unprocessed audio to a worker-owned `Receiver`.
While searching, it retains two seconds of overlap for split headers; after a
supported header it announces the mode before frame assembly and preserves the
picture start between previews. Complete frames are returned once, consumed from
the pending buffer. Every frame has a session identity shared by its partial
previews. The UI saves every nonempty frame with QSaveFile, updating one PNG per
identity as rows arrive. Completed frames, interrupted previews and WAV decodes all
save automatically. Atomic replacement preserves the previous checkpoint on write
failure; the RX panel retains the save error. Identical pictures in different
transmissions still get separate files. Thumbnails update without reloading the
whole directory for each preview.

The remaining audio can contain another mode's header. Finish recording saves
the optional WAV while preserving the input and in-flight preview. Failed image
saves retry while reception remains active, even without new samples. Stop cancels
the worker and discards
the session while preserving saved image checkpoints. Manual receive uses the same
session pipeline and keeps its chosen mode; an explicit picture start applies only
to the first frame. Auto ignores stale manual start values.

This still uses batch demodulation of the pending frame for previews, rather than
preserving FIR/filter state per sample block. An incremental DSP pipeline and
recovery from a new transmission interrupting an incomplete picture remain future
work. Header recognition checks parity or N-VIS checksums and skips unsupported
identifiers in Auto; it does not infer a mode from headerless picture audio.

`src/core/templates.cpp` renders four native QPainter station/weather card styles,
and `exchangetemplates.cpp` supplies seven SSTV contact/exchange designs
from explicit, per-image fields. The renderer never fetches weather or reads a live
clock; only the editor's Now action samples current time. Local/UTC switches
preserve the report instant and °F/°C switches convert the entered value.
`TemplatesDialog` previews at the current mode's dimensions. MainWindow composes
the template, its integrated station callsign and the optional existing caption
before SSTV encoding. This avoids a second callsign overlay
and keeps preview, full screen and actual encoded pixels consistent. Template
fields remain separate from the original photo and caption; selecting a picture
clears the report and caption. No report content or caption text is persisted;
startup removes older saved content. Only design/unit/zone and caption styling
preferences remain. Date/time are optional blank text fields with explicit format
validation and a Now button. Clear all fields resets the draft, including caption,
and Cancel preserves the applied image. Templates are inactive by default.
Tests cover time refresh/conversion, validation, clearing and settings migration, all
mode sizes/mono rendering, cancellation, picture restore and actual SSTV decode.


Template catalog metadata separates weather cards from SSTV exchange cards.
Exchange cards provide CQ, reply, picture report, thanks, your-turn, 73 and photo
frame layouts. Recipient callsign, report, message and timestamp are per-image
fields and never persisted. A QImage background travels from the current TX image
through the editor and renderer; the checkbox decides whether the exchange card
uses it. Choosing a picture modifies only the draft until Apply. Weather rendering
ignores the optional picture and exchange fields, preserving its original output.
The editor hides irrelevant weather/report/photo controls for each design. Tests
cover category selection, blank fields, recipient/report pixels, photo composition,
unchanged source files, all mode sizes and complete GUI/SSTV roundtrips.


`captionpreview.cpp` maps mouse input through ImageView's actual painted image
rectangle, accounting for aspect-ratio letterboxing. It tracks a grab offset in
source pixels, so dragging does not snap text to the pointer. The composition
renderer reports the caption's tight ink bounds and legal placement area; free
placement uses normalized image coordinates and clamps the block inside it.
Template renderers expose individual text bounds and optional per-image positions.
Moved template text is deferred until after the artwork, keeping it visible over
panels. The same image composition feeds preview and encoding. Editor selection
outlines are drawn only by the widget, never in the transmitted image. Reset,
Cancel, new-image clearing and non-persistence follow the existing draft rules.
Mouse tests cover captions, template fields, resizes, letterboxing, bounds,
monochrome modes and complete GUI/SSTV transmission of a dragged layout.

## Incremental workflow features (0.12)

`audio_wizard.cpp` owns isolated ALSA discovery and test processes. It discards
input audio after metering, bounds tests with timers, kills processes on page
changes/close and commits device selections only on Finish. It never calls Hamlib
keying; the final page points to the existing bounded radio controls.

`core/fskid.cpp` independently validates leader, six-bit symbols, marker,
terminator, XOR and callsign syntax. `Receiver` retains a bounded trailer window
after each legacy frame and emits callsign-only updates with that frame's ID.
These do not redraw or autosave an empty image. The UI attaches validated IDs to
the corresponding PNG; Reply deliberately creates a blank new card addressed
to the station. A new frame clears the old received call.

`contactlog.cpp` stores versioned JSON under `contacts/`, using QLockFile for
read-modify-write and QSaveFile for atomic replacement. Picture copies get UUID
names; failed contact saves roll back new copies. The editor uses explicit Save,
blank reports and editable UTC/frequency. CSV exports quote values and preserve
Unicode. Received PNG metadata records mode, reception time and known frequency.

`core/layouts.cpp` stores named layouts under `layouts/`, with versioned JSON,
normalized text positions, color/style values and optional bounded PNG artwork.
Serialization explicitly whitelists layout properties and excludes every report
field and caption text. Picture metadata is stripped. Loading starts from empty
TemplateFields, restores geometry/style only, and rebuilds the gallery model to
keep mouse hit-testing consistent. Default render output is unchanged when no
color overrides are set.

## Classic pattern library (0.13)

`core/patterns.cpp` draws an offline catalog of ten test images. The original
`testPattern()` implementation remains unchanged; the catalog delegates to it for
`test-pattern`. New charts use a 4:3 RGB canvas, then the existing fit/composition
path for the selected mode. `patternsdialog.cpp` provides thumbnails, large
previews and contextual descriptions. The dialog commits only on acceptance.
Pattern selection and user-photo loading both clear the previous image fields.
The bottom stock list is a separate user-photo library (see below). No DSP,
PTT, audio timing, receive logic or on-air mode framing changes in this release.
CLI catalog/export options use the same renderer; default export stays unchanged.
See TEST_PATTERNS.md for source references and RGB rendering limits.

In 0.13.1 the ordinary station callsign is painted on a transparent layer, with
a thin black glyph outline and white text. Pixels outside the glyphs/outline
retain the fitted source image. Caption placement continues to reserve space
for the callsign text. Composition still feeds the TX preview, full-screen
viewer, exported WAV and audio transmission through the same path.

## Draggable station callsign (0.14)

`CallsignPlacement` holds per-image normalized coordinates; the composition result
reports the tight outlined callsign bounds. Free placement crops the transparent
letter layer and clamps the entire text rectangle inside the image. Default
placement retains its previous origin. The callsign remains part of the same
composition that feeds the TX preview, WAV and audio. Since 0.16.1, caption
layers can cover it.

The main TX and TX-fullscreen viewers reuse CaptionPreview hit-testing and grab
offsets. Ordinary moves update CallsignPlacement; template moves update the
existing `textPositions["callsign"]`. Both previews share the current state.
Text-region hit-testing is disabled while audio is being prepared, received or
played and while Hamlib PTT is active. The fullscreen viewer uses the existing Esc and Space shortcuts; its cursor
visibility policy is described below.

The text editor holds a draft placement and commits only on Apply. Caption and
callsign regions have distinct drag keys; moving the callsign leaves the caption
position alone. Right-click reset restores a TX callsign's default position. New
images clear ordinary placement through the existing field-reset path. No extra
per-image text/position preferences are saved. Explicit saved template layouts can
still retain their template callsign geometry.

## TX fullscreen pointer inactivity (0.14.1)

The TX fullscreen window starts with a blank cursor on every entry. Its image
viewer and progress widgets have mouse tracking and an event filter. A changed
global pointer position reveals the cursor and restarts a precise, single-shot
3000 ms timer. Duplicate movement events at the same position do not prolong it.
The timeout hides the pointer and the viewer-only selection guide. Hiding or
closing fullscreen stops the timer and resets the next entry to hidden.
The main TX and fullscreen previews have no callsign-drag tooltip.

CaptionPreview remembers the desired arrow/open-hand/closed-hand cursor while
hidden, so revealing it preserves callsign hover and drag feedback. Normal
previews and RX fullscreen retain their existing pointer behavior. Encoding uses
transmitImage/composition pixels, never a window or screen capture; the cursor
and selection guide do not enter the prepared image or WAV. A simulated send
started with the pointer visible is decoded and compared with the prepared image.

## User photo library (0.15)

The bottom Transmit pictures tab contains user-loaded file paths only. The pattern
picker remains the sole built-in chart chooser. A blank photo library stays empty
on startup, and choosing a test pattern never adds a library entry.

Load picture and Add photos share the multiple-file chooser and import path. The
first readable image becomes the TX source; other selections are validated via
thumbnails. Exact path duplicates are removed before inserting recent selections
at the front. There is no silent 20-photo eviction. The original image files are
read-only inputs, and the list is stored in `pictures/stock` using QSettings.

Extended selection supports batch removal through a visible button, context menu
and a Delete shortcut scoped to the photo list. Removing entries only updates
the path list; it neither deletes files nor replaces the current TX source or its
text. Ctrl/Shift selection does not load a new TX image. Add/remove controls follow
the existing receive/prepare/playback/PTT idle guard. Source files are decoded with
orientation enabled, and thumbnails preserve their aspect ratio.

GUI checks import multiple files through the chooser, verify an empty library
and picker-only test charts, remove selected entries with the button and Delete,
reopen the app to check persistence, compare original file bytes, and decode a
simulated TX after removing its photo to confirm the current image stays intact.

## TX caption dragging (0.15.3)

MainWindow keeps both caption and callsign bounds from the same transmit
composition. Both normal and fullscreen TX previews expose these regions while
idle. Caption movement updates TextOverlay's free position; callsign movement
updates its separate placement (or the template callsign position). The preview,
Image preparation, WAV export and transmission use the same composed pixels.
Movement locks during preparation, recording and transmission. New images clear
caption text and position through the existing field reset.

Regression coverage moves a message with emoji in both TX views, independently
of the callsign, over ordinary pictures and templates and with no callsign. It
checks resize stability, editor synchronization, movement locking, new-image
reset and decoded simulated transmission against the final prepared image.

## Independent text and emoji blocks (0.16)

MainWindow stores placed captions as an ordered list of TextOverlay values. Its
separate blank overlay holds only defaults for the next draft. Text & emoji opens
a blank draft over the existing layers; Apply appends a nonempty block, while
Cancel or a blank Apply preserves the placed content. No block content or free
position is persisted across application starts or new pictures.

composeTransmitLayers fits the original source once, draws all caption layers in
stacking order over the callsign and applies final grayscale conversion if
required (updated in 0.16.1). The existing single-caption API wraps that renderer. Each
layer returns its own bounds for previews, with stable indices until deletion.
Normal and fullscreen TX drag handlers update only the selected layer, using the
same composition for WAV export and sending. Overlapping captions select
the topmost layer, using numeric drawing order.

Right-clicking a block edits or removes that block. Editing replaces its original
list slot, preserving stacking order; other blocks remain unchanged. The editor
shows all other layers and previews the draft in that slot. Fullscreen editors
are parented to the fullscreen window with a visible dialog cursor, and focus
returns there on close. Menus participate in fullscreen pointer tracking.

Templates retains the list when editing the current card. All caption blocks are
draggable there; Clear all fields clears the list, and Reset text positions
resets every block. Saved layouts continue to exclude caption content. New
pictures, patterns and application sessions start without placed text.

Regression checks add three messages/emoji blocks with a blank draft each time,
move blocks independently in both TX views and the template editor, edit/cancel/
remove individual blocks, reject blank additions and compare decoded simulated
TX with the final composition. Renderer checks cover all modes and overlapping
selection with more than ten layers.

## Unrestricted caption overlap (0.16.1)

Free caption positions clamp to the full image rectangle, without a callsign
exclusion strip or forced inset. Anchored presets retain a small image margin
but no reserved callsign row. The callsign is drawn over the base image before
the caption blocks, so an overlapping caption can cover it.

TextOverlay carries a current-picture stackOrder. Rendering and hit testing use
the same stable ordering, with input order breaking ties. Dragging a caption
raises it above the other captions without changing its list index or drag key.
New drafts start on top; editing preserves order until a drag raises the block.
The behavior is shared by TX, fullscreen, Text & emoji and Templates. Callsign
hit testing follows the visible caption layers. Stacking is not persisted in
settings or saved layouts.

Tests cover free placement at both image corners in every mode, actual caption
pixels covering callsign pixels in every mode, and normal/fullscreen drags that
raise an older block over another block. Simulated TX with two overlapping text
blocks over the callsign is decoded and compared with the expected prepared
image over both ordinary pictures and templates.

## Save transmit pictures (0.17)

The TX Save image button, File action, preview context menu and Ctrl+Shift+S
share saveTransmitImage. Its idle guard matches image editing and radio/PTT
locking. It snapshots transmitImage,  so the
PNG contains the same callsign, text blocks, stacking and template pixels as TX.
The viewer, pointer and selection guides are never captured.

A save dialog supplies a timestamped default PNG name in Pictures/OpenSSTV/Transmit
or the last successful destination folder. The final .png suffix is normalized
before overwrite confirmation. QSaveFile and QImageWriter encode and atomically
commit the PNG; failure or cancellation leaves the photo library and composition
unchanged. Mode, callsign, save UTC and software are included as PNG metadata.

Only after commit does the path enter the Transmit pictures library, without
loading it as a new source or resetting the editable layers. The remembered
folder is saved separately in tx/imageSaveDirectory. Fullscreen save dialogs and
overwrite prompts have visible cursors and return focus to the fullscreen viewer.

GUI tests save through the button, menu and normal/fullscreen shortcuts, compare
PNG pixels with the expected color, monochrome and weather
compositions, and verify metadata, cancel/overwrite/error handling, library
persistence and source/layer preservation. Overwrite tests also check normalized
filenames and removal from the library without deletion of the saved file.

## Received picture-in-picture (0.18)

ImageInset holds a frozen QImage, normalized top-left, width fraction and an
optional frame. composeTransmitLayers fits it within the mode's full image
rectangle, preserving the entire received image's aspect ratio. The frame is
separate from the image content, with no cropping or received-image edits. The
inset is drawn over the background before regular callsigns and text captions;
final monochrome conversion covers the whole composition. Empty insets keep the
existing rendering path unchanged.

RX Add to TX and Receive history's context action seed an InsetDialog with a copy
of the chosen received pixels. File and TX preview context actions edit the
current inset, or use the current RX image if none is placed. The dialog supports
a width slider/spin box, frame, drag placement, file choice, explicit replacement
from current RX and removal. Apply commits; Cancel keeps the previous state.

Main TX and TX fullscreen register the inset's actual composed rectangle for
dragging. Bounds and cursor guides remain viewer-only; the core image feeds SSTV
preparation, saved PNG, contact pictures, WAV and playback. The text and template
editors also show the inset. Templates Clear all fields and saved-layout loading
clear received content; saved layouts never store it. New TX source images and
patterns clear the inset; later RX updates leave it unchanged. All editing
controls obey the existing receive/job/playback/PTT idle guard. Fullscreen editor
focus returns to the fullscreen viewer.

Tests cover every mode's bounds, aspect ratio, edge placement, finite-value
fallback and unchanged source pixels/files; source corners, ordinary and
fullscreen dragging, frame/size changes, saved RX selection, cancel/remove/reset,
new received frames, and exact saved images and decoded SSTV frames. A legacy monochrome
roundtrip verifies that the received inset reaches the transmitted picture.

## Universal radio frontend (0.19)

RadioFrontend is the fourth main viewer tab. It has an independent OpenSSTV
instrument design, with custom-painted dial and signal meter, and uses the
existing single Hamlib Controller. Radio view hides the SSTV side panel and
gallery to give the console the full window width. Navigating back restores
them without changing SSTV modes or image content. Setup, connection, advanced
controls and emergency stop route through MainWindow's existing radio lifecycle.

All displayed frequency/mode/VFO/PTT values come from controller readback. A
full Hamlib read optionally adds calibrated STRENGTH as strengthDb; an invalid
value clears an unavailable meter reading. PTT-only reads retain the existing
small TX polling path. The frontend hides stale readings when disconnected and
pauses the receive meter while transmitting. RF power stays in percent.

Frequency requests from the dial/buttons are debounced into one pending target.
A separate in-flight target preserves additional turns during asynchronous CAT
commands. Targets are cancelled on loss of permission to tune, disconnect, PTT
or explicit stop. Other Set controls send only their own command. Poll updates
respect dirty/focused editors; reconnect initializes them from actual readback.

The top-row Stop / PTT OFF remains available while the instrument area scrolls.
It calls the common stop/release method directly, including manual audio with
no connected Hamlib device. Normal commands obey job/audio/PTT guards. Tab
navigation never connects, applies settings or keys PTT. Advanced controls retain
the existing timed PTT-test behavior.

Simulated-controller tests cover offline navigation, setup routing, telemetry,
independent edits, missing readings, CAT errors, reconnection, coalesced rapid
tuning/in-flight steps, PTT cancellation and Stop during manual audio. Native
Hamlib Dummy and existing radio/PTT lifecycle tests cover the shared backend.

## Audio gain and headroom (0.20)

AudioLevelsPanel is embedded in the RadioFrontend scroll area. It stores manual
RX gain and TX attenuation in QSettings. It owns bounded explicit calibration
processes, with no Hamlib dependency or PTT calls. Application audio state stops
calibration before opening the main capture/playback path. Shutdown is explicit
before MainWindow members are destroyed, and callbacks guard the closed state.

core/audio_levels implements peak/RMS measurements, 100 ms output envelopes,
float output attenuation, and chunk-safe little-endian signed-16 PCM gain.
Unity RX preserves exact bytes, including asymmetric integer endpoints; odd-byte
chunks are carried across reads. Source clipping remains flagged after reduction.
Output attenuation runs after SSTV encoding and Morse append, before WAV writing;
100% is a no-op. MainWindow snapshots the value before starting its worker. TX
meter envelopes use a separate playback clock, without CAT lead/tail time. RX
gain is read on each capture block before the incremental decoder and recording.
It does not affect imported WAV decoding or apply AGC/limiting to generated SSTV audio.

Tests use fake ALSA helpers only. They exercise split samples, unity identity,
gain ratios, source/software clipping, invalid inputs, persistence, check cleanup,
playback failures, actual MainWindow capture/TX/Morse levels and control locks.
Attenuated-then-amplified waveforms decode in B/W and color SSTV modes;
mean pixel changes remain below one grayscale level.

## Additional skins and radio appearance (0.21)

The built-in catalog retains the six original IDs and adds Neon Circuit,
Synthwave, Solar Flare, Ocean Glass, Titanium and Crimson Ops. SkinFinish controls
flat/neon/glass/metal material treatment; an optional secondary accent adds
color-pair swatches and selected-tab detail. QSS uses native Qt gradients and
borders. Settings previews changes immediately and retains existing Save/Cancel
semantics. The compact preview is illustrative artwork, not live telemetry.

Radio appearance now comes from the same skin system. Classic maps to the
existing dark instrument palette; other skins extend into the console, dial and
meters. Custom painters store an explicit palette to avoid partially resolved
QSS palettes giving inactive meter bars the wrong color. Clipping/high-level
meter colors stay semantic, with darker colors for light backgrounds. Radio
state, tuning/PTT behavior, audio samples and image composition are unchanged.

The existing skin preview/persistence workflow iterates all catalog entries,
checks radio palette changes, and can export main/settings/radio/audio previews.
Template tests also exercise each skin. Artwork is drawn locally in Qt, without
third-party texture or image assets.

## Live audio waterfall (0.22)

core/SpectrumAnalyzer uses a Hann window and an in-place radix-2 FFT. Window
sizes scale with sample rate for roughly 6 Hz or finer FFT spacing, and frames
advance by 50 ms of input audio (rounded to sample boundaries). Single-sided
amplitudes are normalized by window sum, converted to dBFS and bounded to −90..0.
Each output column takes the strongest FFT magnitude in its frequency interval
across 500–3000 Hz. Sample-rate changes reset the stream; static tail analysis
uses independent state. FFT/ring/window storage is fixed for a given rate.

SignalView retains at most 600 spectra (30 seconds at 20 frames/s). Newest frames
paint at the top; vertically compressed history uses maxima so short events are
not discarded by row skipping. Colors follow the selected skin. Freeze keeps a
bounded snapshot while live history continues; Clear resets display/analyzer
state only. Stop preserves history, and a new input session clears/unfreezes it.
Spectrum uses the latest live frame or an explicit WAV/encoded-audio tail preview.

MainWindow feeds adjusted PCM directly from readCapture, before the decoder
worker, and never re-feeds decoder results as live audio. The input check panel
emits its existing adjusted capture stream and lifecycle signals to the same
monitor. No extra capture process, Hamlib action, or audio gain/filter is added.
The single monitor widget moves between the SSTV sidebar and Radio console;
sidebar controls scroll and the RX/Stop transport row remains outside the scroll.
Normal input callbacks feed the FFT even when receive decoding is busy.

Focused tests cover tone frequency/amplitude at six rates, silence/two-tone
separation, capture-chunk invariance, sample-clock frame timing, reset/invalid
samples, bounded history, display freeze with ongoing capture, odd PCM chunks,
RX/input-check lifecycle, device failures, saved view choice, tab reparenting and
minimum-window layout. Synthetic audio helpers replace physical devices in tests.

## Independent picture and Morse identification (0.23)

Station/audio has independent Callsign on image and Morse ID checkboxes, also
available in the TX context menus and the Settings identification group. Defaults
preserve the existing visible callsign and the saved Morse preference. The image
visibility preference persists as tx/showCallsignOnImage; the actual station
callsign remains stored and feeds Morse validation/timing/audio unchanged.
Settings edits commit both flags together on Save, and Cancel leaves them alone.

Image composition uses an empty automatic callsign when hidden. Template rendering
has a default-true showCallsign argument so hiding omits the station field and its
placeholder, preserving other station/report text and all artwork. Template
thumbnails/editor, text editor, inset editor, normal/fullscreen previews, image
save and encoder preparation share the preference. Callsign bounds/hit regions
are absent while hidden, and saved placement survives re-enabling the checkbox.
Morse remains appended to the same generated WAV, through the existing PTT lead,
audio/ID, tail and release lifecycle. Manual captions or lettering already in a
source photo remain image content.

Focused tests cover checkbox/default/persistence behavior, Save/Cancel/sidebar
sync, context menus and editing previews, every template's station/placeholder
suppression, and actual BW8/Robot sends with a hidden image callsign and intact
stored Morse ID. The BW8 image waveform matches a clean-image encoding within
PCM quantization tolerance. Both retain the expected
Morse waveform at the end of the played WAV. Tests use a fake player, not RF.

## TX composition Undo/Redo (0.25)

TxEditState captures the current source, frozen image inset, captions with their
stack order, callsign placement/visibility, template data/enabled state and last
pattern selection. Qt implicit sharing retains source/inset images without a
full bitmap copy for every text move. QUndoStack retains at most 50 commands per
current picture; explicit photo/pattern loads clear it. Radio/audio/Morse,
station callsign, received images/history, gallery files and editor draft-style
preferences remain outside the document state. Visibility is an image edit;
undoing it updates the checkbox while retaining the station's actual identity.

A command stores before/after snapshots and skips its initial redo because the
UI already applied the edit. Only subsequent Undo/Redo restores a snapshot,
blocks checkbox signals, saves relevant preferences and runs the existing
updateTx render/prepare path. There is no raster-only restoration or change to
mode/profile selection, RF state, saved files or audio. No-op and cancelled
changes do not push commands. New edits drop the redo branch via QUndoStack.

CaptionPreview emits gesture boundaries so all move events form one command.
Clicks without movement do not reposition text. Mouse ungrab, hiding or loss of
a draggable region finalizes an interrupted gesture; starting RX/TX or an editor
also commits the current drag before locking or changing the document. A
visibility change interrupting a drag is recorded separately from the move.
Editor acceptance, template clearing and inset changes each commit one snapshot.

Shared QAction objects provide Edit/context menus and window-scoped shortcuts
on MainWindow and the TX fullscreen window. The two toolbar buttons reside in
the existing progress panel, which moves between those windows. Actions/handlers
obey existing job/RX/playback/PTT guards and disable during a drag. Native field
ShortcutOverride/Undo stays active in modal editors and station text fields.
Shutdown blocks/clears the stack and disconnects gesture signals before member
teardown. Tests cover state restoration, image saves and actual SSTV playback,
one command per drag, interrupted gestures, history limit/branch/reset, native
text Undo, fullscreen keys and RX/TX locking with synthetic audio helpers.

## Audio device setup (0.26)

`AudioWizard` parses ALSA `arecord -L` / `aplay -L` blocks into names and readable
descriptions. Its normal selector intersects capture/playback endpoints by stable
CARD and DEV identifiers and prefers plughw over sysdefault aliases, leaving
sample-rate, format and channel conversion to ALSA's plug layer. Output-only,
input-only and null endpoints stay out of the combined selector; Advanced keeps
the complete lists and manual names. Duplicate descriptions show distinct card
IDs. No manufacturer/model is inferred from USB Audio CODEC.

Discovery runs asynchronously with a 2.5-second process limit and explicit retry.
It preserves current choices during loading, refresh, failure and unplug events.
Only an explicit user selection changes the RX/TX pair. Existing manual and
hw aliases are retained exactly until the user chooses a new pair. Test pages
show the selected devices, stop audio on navigation/close, and retain the bounded
receive meter and one-second low-amplitude output check. Discovery never opens
audio streams. SignaLink and built-in-radio profiles provide different setup text.

Finish commits the connection profile and audio names. The SignaLink PTT checkbox
can explicitly disable `radioConfig_.automaticPtt` without connecting, keying or
replacing other Hamlib settings. Other profiles leave PTT alone; Cancel commits
nothing. Connection profiles never change a radio's own USB audio menu settings.

## Playback mixer optimization (0.27)

`playback_mixer.cpp` uses bounded, argument-only amixer processes on the wizard's
Qt Concurrent worker. It requires an explicit PCM card and USB mixer identity;
it never resolves a system-default/custom route by guessing. The helper saves
raw kernel-control values for supported PCM/Speaker/Master/Headphone playback
controls, then uses playback-only simple-control commands for normal output
and unmute. Common capture/playback controls are excluded. It verifies simple
control readback and rolls back a failed/partial adjustment. Capture values
are never set. Each command has a 1.5-second timeout plus bounded termination.

The wizard retains the first snapshot for each touched card, including on
repeated optimization. It disables tests, device edits and navigation while the
worker runs. Closing waits for that bounded job and restores the snapshots on
Cancel. Finish retains only a successfully optimized, currently selected
SignaLink card and restores other cards. Restoration checks mixer identity,
uses exact per-channel raw values/mutes, then verifies them. The main window
reports restore errors instead of claiming rollback succeeded.

TX percent is a wizard draft until Finish. The AudioLevelsPanel setter commits
it to both existing radio-tab controls and settings, so subsequent tones and
SSTV sends use the same value. Optimization sets that draft to 100; it does not
play a tone or automatically operate Hamlib PTT. Receive gain remains independent.

## Automatic reception and RX clipping protection (0.29)

`mainwindow_audio.cpp` reconciles reception on the GUI event loop. Reception is
requested on every launch, independently of the retired opt-in startup setting.
There is no Live audio control. Background reception feeds the existing
incremental Receiver through the same arecord, gain, meter and waterfall path as
optional WAV recording. Every second, consumed PCM moves into Receiver and is
removed from the GUI buffer, so quiet listening does not accumulate recordings.
Receiver retains only the header overlap or the current image needed for decode.
Background queued audio has a ten-minute bound that restarts capture if exceeded. No WAV is saved until
Record audio is selected. Promotion preserves Receiver and split-byte PCM state;
Finish recording saves the WAV and discards only already-fed GUI audio without
closing the stream or replacing Receiver; partial-image timing survives.

TX preparation/playback, reported/owned PTT, Settings and the audio wizard pause
background reception. Reconciliation resumes it when idle, including after Stop.
Input failures retry automatically with bounded backoff; there is no session
opt-in flag. Device/receive-option changes start a fresh receiver; generation
checks ignore obsolete capture results/notifications.
The audio-level panel prevents a second input check while RX runs, but permits
the device wizard during background RX. Closing prevents queued work.

PcmInputGain optionally measures raw PCM first and reduces the requested gain
when the predicted peak exceeds -1 dBFS, targeting -6 dBFS. The entire block uses
that gain; split samples are buffered as before. Silence never increases gain.
AudioLevelsPanel persists/shows reductions and enables this protection by default.
Source clipping remains flagged even after software attenuation. Repeated source
clipping starts a bounded asynchronous capture-volume reduction on the explicitly
selected ALSA card. Only writable integer Capture Volume controls are selected;
values drop by ten percent of the control range at most once every two seconds.
The worker verifies card identity before writing and checks readback. It never
changes playback, switches or ambiguous default routes. Unsupported/minimum
controls show manual RX/input guidance. Shutdown waits for the bounded worker.


## Receive frame boundaries and rearming (0.29.1)

Completed legacy frames report the actual final sample position after per-line
sync tracking, rather than a nominal end derived only from clock scale. Receiver
therefore preserves following headers/FSKID even when accumulated sync corrections
shorten the first picture and the next header straddles input blocks.

Receiver resets picture timing, frequency lock, control deadline, header scan
cursor and first-frame manual start after every completed frame. It preserves
unconsumed audio and late FSKID context. For interrupted pictures, it scans new
audio with two seconds of overlap for a validated fresh VIS/extended/N-VIS
header. Preemption requires the full preamble. If that header precedes the old picture's expected end, the previous
frame is checkpointed only up to the new header, then the receiver rearms with a
new frame identity and retains the complete incoming preamble. The audio process,
waterfall and optional WAV recording remain open. The last saved picture remains
visible until the next image starts decoding.


## Clean caption composition (0.30)

Caption/callsign fills and alpha-derived outlines render in separate premultiplied
ARGB layers at 4× resolution, with grayscale antialiasing and image-based font metrics.
The outline is calculated from the high-resolution alpha before fill and outline
are reduced once to the mode canvas. The fitted background is never enlarged or
resampled by text rendering. Dragging continues to recompose from the original
background/layers, so old positions leave no marks. Color emoji remain color.
A bounded 32 MiB cache of native caption fills/outlines stabilizes pixels across
font-cache warming in editors and avoids rerasterizing text during dragging.

## Webcam selfies (0.32)

SelfieDialog owns a video-only GStreamer helper process. Camera discovery queries
Linux V4L2 capabilities and excludes metadata/output nodes. The selected camera
feeds decodebin → videorate → videoconvert → videoscale → packed 640×480 RGB at
10 fps → stdout. The Qt process consumes complete fixed-size frames, retains the
latest preview, and carries incomplete bytes to the next read. It adds no JPEG
encoding stage. Preview/save mirroring applies the same integer pixel transform.

The camera opens only when the selfie dialog is requested. Capture freezes the
latest frame and kills/reaps the helper; Retake starts a new preview. Cancel,
close and destruction always stop it. Deferred startup checks the closed state,
so cancelling before the event-loop startup cannot open the camera afterward.
A startup timeout and subsequent frame watchdog reject missing/stalled video;
busy/permission/helper failures show a recoverable error with capture disabled.
Errors are bounded to 2000 characters. Missing cameras can be refreshed.

MainWindow pauses background receive while the modal capture window is active,
and resumes it on all exit paths. Only Use photo commits camera/mirror choices
and saves a timestamped PNG atomically under Pictures/OpenSSTV/Selfies; successful
saves use the existing photo import path. If saving fails, the accepted image is
kept in TX memory with a save-copy message. Cancellation preserves the current TX
picture and writes no photo. The camera flow does not start audio output or PTT.
GStreamer tools/base/good plugins are explicit installer dependencies; Qt
Multimedia development libraries are not required.


## Explicit RX mode selection (0.32.1; superseded by 0.33)

Mode buttons and dropdown activation can request an immediate input retry while
preserving an existing live receiver. Since 0.33, input also retries automatically;
RX mode selection is never required to enable reception.


## Continuous automatic receive (0.33)

Removed receiveRequested: reception is always wanted unless closing, temporarily
suspended by a dialog, or transmitting/PTT is still held. Stop cancels the current
operation and input/decoder generation, then reconciliation creates a new receive
session. Auto remains the startup mode and no button activation is required.

An input exit/start failure schedules a single-shot retry at 1, 2, 4, 8, then
10-second intervals. Reconciliation respects this timer and normal worker/TX
exclusion. Nonempty PCM resets backoff and clears the previous error. Input changes
can retry immediately. A close stops the retry timer and prevents queued restarts.
Input failure cannot leave the application permanently disarmed. A stalled GUI
audio queue also cancels/restarts its receiver rather than requiring a mode click.




## Supported modes (0.35)

The program contains the 43 MMSSTV target modes. Mode buttons, the catalog,
CLI encoding and automatic receive use that same set. Removed experimental
digital modem/profile controls, compression dependencies and protocol-specific
receive paths. Continuous reception, image composition and saved PNG history
remain independent of mode selection. Obsolete profile settings are discarded.


## Responsive editing during automatic RX (0.36)

Live capture decoding uses receiveJob/receiveCancel, separate from the foreground
TX/WAV watcher/token. Foreground busy locks no longer apply to automatic receive
workers. Capture generations reject old completions and progress after input
changes, modal pauses, Stop or TX. Stop/close cancel both workers; destruction
waits for each. Starting TX cancels background receive without blocking the GUI
on its decoder. Input reconciliation avoids overlapping pushes to one Receiver.
Progress posts are deduplicated/throttled to approximately 60 Hz and ignore
cancelled/stale receive work or foreground output preparation.

RX delivery updates only RX viewers. Resizing repaints the existing mode canvas
without recomposing TX. Position-only edits update artwork/hit regions instead of
rebuilding duration and every control's enabled state. These paths leave gestures
and one-command-per-drag undo boundaries intact.

Composition uses bounded thread-local caches: 16 MiB for fitted backgrounds keyed
by source image cacheKey, dimensions and monochrome status; 4 MiB for callsign
fill/outline/crop keyed by text, canvas width and resolved font; and the existing
32 MiB caption cache, now including tight crop bounds. Position is applied after
raster lookup. Source mutation changes its key; returned QImages detach before
painting, so neither callers nor overlays can corrupt cached background pixels.


## Remote station transport (1.1)

`remote_station` uses an ordered byte stream with bounded length-prefixed JSON
and binary frames, over Qt TCP in normal operation. A fresh server challenge
authenticates the configured random connection key using HMAC-SHA256. Derived
session keys authenticate every subsequent message and its sequence number.
This provides integrity and replay detection, not encryption. The service is
intended for a trusted home LAN. Exactly one paired laptop owns controls.

Receive data is raw mono signed-16 little-endian PCM at 12000 Hz from the shack's
existing capture process, before software receive gain. The laptop feeds its
existing receiver and waterfall, including normal automatic saves and recording.
SSTV reception never opens a local ALSA input in client mode, including after
a disconnect. The separate voice PTT path opens the laptop microphone only
while voice PTT is explicitly locked on.
Client software RX gain is independent; automatic hardware input adjustment stays
on the shack. Foreground TX uploads a complete mono 48000 Hz WAV in 64 KiB chunks,
with length, SHA256, format and duration validation before any PTT or audio.
A 64 MiB/ten-minute waveform cap, 128 KiB frame cap, 8 KiB JSON cap, bounded TCP
output queues and pending requests prevent unbounded buffering.

`mainwindow_remote` adapts the existing `radio::Controller` through asynchronous
requests. Actual radio readback is supplied by the shack; client connection and
PTT are separate from the network connection. The shack retains local Hamlib
sequencing, cancellation generations, error handling and playback watchdogs.
Connection loss and a six-second control heartbeat lease call the existing stop
path. A direct remote PTT test is additionally capped at ten seconds. Session
generations discard late radio replies after an owner changes. Per-transmission
tokens discard stale completion events after Stop and immediate retry. Reconnect never
requeues or resumes TX. The GUI allows a single controlling laptop and keeps
local Stop available while other local hardware controls are locked.

`remote_tests` exercises framing, pairing, signing, replay rejection, ownership,
waveform transfer, cancellation and heartbeat handling through a queued duplex
QIODevice. A separate loopback check exercises real TCP where allowed.
`remote_ui_tests` uses two real MainWindow instances, fake ALSA executables and
a radio backend to check decode/display, CAT/MV forwarding, shack-only playback,
Stop, disconnect, reconnect and SignaLink operation without CAT.


## Shack remote TX monitor (1.1.1)

`TransmitMonitor` loops the accepted remote waveform into a separate Receiver.
It starts on the shack player's `started` signal, reads only elapsed playback
samples from the validated canonical 48 kHz mono WAV and decimates to the live
receiver's 12 kHz input. A 250 ms GUI timer schedules one background decode at a
time. An explicit busy flag keeps a queued future completion from being replaced
before its result is consumed. Worker generations and cancellation isolate Stop,
new transmissions and shutdown. Complete playback drains the final samples;
interrupted playback never fast-forwards the rest of the image.

Only the latest second of catch-up PCM is delivered to the GUI waterfall. Decode
work never joins the foreground job or claims antenna capture, and monitoring
never changes PTT or audio output. Actual RX settings do not affect the clean
loopback. Regular capture resumes through the existing receive state machine.

The shack switches to RX when remote playback starts. Both RX image views show
its progressively decoded image, tagged as a TX monitor. The monitor does not
write automatic receive history or claim an over-air reception. A new actual
receive frame or history selection cancels any pending monitor delivery. Save
copy preserves the displayed picture; explicit contact logging offers it as a
TX image. Tests cover timing, final drain, interrupted/next images, silence and
two-window remote UI playback with a fixed unrelated antenna RX mode.


## Bidirectional station operation (1.2)

The shack UI can transmit while a laptop is paired. `Server::reserveLocalTransmit`
reserves the physical station before the local encode starts. Laptop uploads
are rejected while reserved; a local reservation is rejected after a laptop
upload is accepted. Existing main-window job, PTT and playback guards remain
in force. Completion, Stop and preparation failures release the local claim.
The two interfaces can use idle radio controls, with hardware operations still
serialized by the shack Controller. Hardware connection settings stay locked
while paired.

For a local shack TX, TransmitMonitor reads played samples from the WAV and
sends every sample through an authenticated monitor stream. The laptop starts a
fresh TransmitMonitor in stream mode, automatically shows RX and feeds its
background Receiver with incoming 12 kHz PCM. Normal RX PCM sequence numbers
remain separate. Stream IDs prevent stale ends or data from affecting the next
image. Successful EOF drains queued data; interruption preserves the partial
image. Only the latest second is plotted in the GUI during catch-up.

The signed welcome advertises duplex-monitor support. New clients then advertise
it in a signed capabilities message. Unadvertised clients receive no new stream
messages, preserving the existing one-way protocol without adding an insecure
capability flag to the plaintext pairing exchange. Both endpoints need version
1.2 for bidirectional monitoring. The radio always plays locally at the shack;
this visual audio loopback does not claim RF reception by a second radio.

## Voice PTT (1.3.1)

`voice_audio` owns separate ALSA capture/playback processes using 48 kHz mono
S16_LE. Capture emits 20 ms frames with selected gain and a fast-attack limiter.
Playback waits for PTT acknowledgment plus configured lead, primes a 120 ms
jitter buffer, then feeds bounded frames to aplay. Its buffer is limited to
500 ms; an output backlog, malformed audio or 1.5 seconds without fresh PCM
ends the stream. Initial setup allows five seconds; each voice session is capped at
three minutes. Capture has its own two-second stalled-microphone watchdog.

`remote_station` advertises voice in its authenticated welcome. Voice begin is
acknowledged asynchronously after opening output and keying. Audio frames carry
a stream token, 48 kHz rate and exact byte offset under existing message MACs.
Voice and SSTV use a shared reservation. End, connection loss, local Stop or
failure invalidate the token and cancel pending begins; late acknowledgments
are ignored. The laptop drops pre-ack capture rather than playing stale speech.

`mainwindow_voice` keeps local microphone settings separate from remote radio
RX/TX devices. The Radio tab uses a checkable button: the first click opens
capture and locks PTT on, and the next click stops it. Mouse release and tab/focus
changes preserve the lock. Closing, Stop, disconnect and errors clear it. The
checked state follows the active session and is never persisted across restarts.
The GUI's voice generations cancel delayed capture,
key and lead callbacks. Stop kills voice playback before requesting PTT OFF;
existing uncertain-PTT state continues to block new TX. The normal SSTV receiver
pauses during voice and restarts afterward. Speaker RX monitoring is added separately in 1.9.0.

`voice_tests` exercises real GUI/controller/network code with fake ALSA processes,
a fake radio backend and an ordered in-memory connection. It verifies exact PCM,
gain/limiting, buffering, errors, missing packets, reservation conflicts, stale
sessions, startup cancellation, local and remote SignaLink operation, disconnect,
Stop, click-to-lock/unlock, keyboard toggling and focus/tab changes while locked.
These checks do not establish real ALSA, Wi-Fi or RF performance.

## Hamlib setup over the shared station (1.3.2)

`mainwindow_radio_setup` provides a dedicated Hamlib dialog for a remote laptop
and for a shack with a paired client. Local unpaired operation retains the normal
settings Radio tab. The main Radio menu exposes the same entry point. A signed
snapshot carries `radioConfig`; its presence identifies hosts supporting remote
setup. Old or disconnected hosts show an unavailable hint rather than the
laptop's local configuration. General client settings link to the shack dialog.

`radio::Config` supplies typed bounded JSON serialization. `radio.configure`
accepts a complete configuration and the expected prior snapshot. The shack
rejects stale forms and invalid fields. Applying setup checks station idleness,
reserves the shared transmitter and publishes a preparing state, then closes
any active/pending CAT connection on the serialized controller worker. It saves
only after close succeeds and the requesting session remains current. The
reservation is released on completion or error. No auto-connect or PTT ON occurs.
Local laptop configuration is never replaced by the shack's form. Dialog reply
callbacks use QPointer guards after close; actual hardware and Wi-Fi performance
remain outside the simulated tests.

## Shared-station audio setup wizard (1.3.3)

`mainwindow_audio_setup` keeps a single audio setup session per station. A native
shack wizard or a remote session pauses normal capture and reserves the shared
transmitter until completion. The same AudioWizard implementation performs ALSA
discovery, RX metering, test tones and transactional USB mixer optimization on
the shack. The hidden server wizard never opens radio controls or requests CAT
PTT. Explicit test tones retain the existing VOX warning and duration limits.

`audio_wizard_relay` mirrors named controls into a client AudioWizard constructed
in relay mode. All local discovery/capture/playback/mixer entry points are disabled
in that mode. Typed edits, page changes and explicit test actions go through the
existing authenticated RPC channel. Bounded compressed JSON state contains at
most 64 choices plus the selected item per combo, preserves backend choice IDs,
and carries live labels, level readings and button states. Clients reject
compressed states over 4 KiB and advertised uncompressed lengths over 256 KiB. Revision tracking drops
stale replies after newer edits; polling pauses during Finish and sends at most
one outstanding refresh. State responses omit unrelated station snapshots.

RPC result callbacks have the same deadlines, combined pending-request limit
and disconnect completion behavior as existing commands. The setup lease has a
session and wizard identity; stale commands cannot close a later wizard. Finish
uses the existing mixer commit and saves only the shack's devices/PTT choice.
Cancel and heartbeat/connection loss use the existing mixer rollback. Restoration
errors remain visible on the shack and are returned to the laptop. The laptop's
local device profile is untouched; normal receive resumes after cleanup.

Tests exercise the actual two-window wizard flow with fake ALSA/mixer helpers
and an authenticated ordered in-memory link: discovery, input meter, selected
USB output tone, optimization, Finish, Cancel, disconnect and failed rollback.
They also prove relay mode does not open laptop audio and that silent result
replies preserve the current station snapshot. Physical USB, Wi-Fi and RF still
require hardware verification.

## Setup entry points and target selection (1.3.4)

`mainwindow_setup_access` centralizes setup readiness. Menu and device-setup
entry points stay enabled. A busy station shows a choice to preserve the active
operation or stop and open setup. The latter waits up to ten seconds for jobs,
voice, playback, recording and PTT state to clear, then queues the original setup
request. An unconfirmed stop does not open mutable hardware settings. Existing
visible audio wizards are raised instead of duplicated.

General Settings no longer rejects a paired host. It reserves the shared
transmitter, publishes a preparing state and uses dedicated hardware links when
sharing or remote. Scope guards release the reservation and resume receive on
all exits. The General page uses a scroll area. A connection banner and local
USB shortcuts clarify where hardware edits apply. Explicit local switching
copies the remote configuration and changes only its role, retaining the saved
host/key. Offline remote audio and radio pages provide this path directly.
Tests verify paired settings, local USB recovery from offline remote mode,
TS-990S model selection reaching a fake backend, and locked PTT remaining on
until the user chooses Stop and open setup.

## Tux-sstv branding and compatibility (1.4.0)

`branding` provides the display name, original penguin artwork and icon resource.
The canonical executable/package name is `tux-sstv`. An installed `opensstv`
wrapper and a build-tree symlink preserve old command invocations. Existing
`opensstv.desktop`, AppStream ID and StartupWMClass remain stable, while Name,
Exec and Icon present Tux-sstv. The Debian package declares Provides, Replaces
and Conflicts for the old opensstv package.

Qt application/organization storage names intentionally remain OpenSSTV. This
preserves QSettings and QStandardPaths application data without copying or moving
user files. ApplicationDisplayName and explicit window titles use Tux-sstv; CLI
parsing temporarily uses the display name for Qt’s standard help/version/error
handling, then restores the storage identity before any profile access. Picture folders retain
OpenSSTV paths for history/selfie/save-directory compatibility. Newly saved image
Software tags use Tux-sstv. Authentication domain strings also remain unchanged,
so a branded client can pair with older versions using the existing key.

Penguins are SVG resources rendered as ordinary widget/header/icon pixmaps.
They are never passed into the SSTV image composition or modem pipeline. The
package includes Qt's SVG plugins for consistent rendering. Tests verify the
About developer credit, icon/header resources and new/legacy command output;
release checks load a seeded legacy profile in an isolated XDG environment.

## Unified Setup wizard (1.5.0)

`MainWindow::showSetupWizard` coordinates station connection, audio, CAT/Hamlib
and a saved-settings summary. The station page validates and applies a changed
`remote::Config` before choosing the hardware target. An unchanged connection
is retained, including an authenticated laptop session. Pending local CAT
closure keeps navigation disabled until the connection change completes.

The audio step runs the existing native/relayed four-page audio check. Device
selection, mixer rollback, explicit tones and shared-station reservations keep
their existing behavior. During remote audio opening and completion, the outer
wizard waits for the child session. Live reception continues while browsing
Setup and pauses only for the audio check. The relayed audio page IDs and wire
protocol are unchanged, so older supported shack peers remain compatible.

The CAT step embeds `RadioSettingsPage`, reads either the local configuration or
the authenticated shack snapshot, and saves through `applyRadioSetup` or
`radio.configure` with the expected original configuration. Remote reconnects
invalidate the displayed CAT session. A save waits for acknowledgement before
advancing; invalid or busy changes remain on the page. Connection checks use
Radio → Connect after setup. No automatic CAT connection or PTT is introduced.

Settings commit per section, as explained on the station page. Closing Setup
keeps already saved sections; cancelling the audio check still rolls back its
uncommitted mixer changes. `setup_tests` verifies cancellation, validation,
local CAT saves and remote target/profile separation. Audio and waterfall tests
follow the unified entry and verify audio save/cancel and automatic RX recovery.


## Additional weather readings (1.6.0)

`TemplateFields` includes optional numeric wind speed, rainfall, humidity and
barometric pressure, with explicit wind/rain/pressure units. Validation applies
to weather designs only; exchange cards ignore hidden readings. Settings and
`LayoutStore` persist unit preferences and placement but exclude reading values.
Missing unit keys in older profiles/layouts use mph, inches and inHg.

Weather rendering retains its original geometry when these readings are blank.
With readings, the main panel grows to fit a two-by-two grid, the temperature
and weather icon shrink, and date/time move below the panel. Every supplied
reading has its own template text key and uses the existing deferred text path
for dragging. Blank readings produce no label or hit region; numeric zero is
retained. Dialog unit changes convert values, and a scroll area keeps the form
accessible while preview and Apply/Cancel remain outside the scroll area.

Template tests check numeric ranges, fresh fields, unit conversions, nonoverlap
at multiple image sizes, manual placement, visibility, clear/cancel, undo/redo
and the actual generated WAV through simulated playback and decoding. Layout
tests check that unit/placement choices survive without retaining readings.

## TV Studio cards (1.7.0)

`ImageTemplate::tvCard` distinguishes five television designs from weather and
exchange cards without changing the existing weather/exchange filter indices.
TV Studio is gallery category 3. `renderTemplate` dispatches TV cards to the
pure `drawTelevisionTemplate` painter before the exchange renderer. Native
geometry, text and optional background photos scale from the same 640×480
canvas and share the existing movable/deferred text helper.

Headline, subheadline, ticker, program information and home/away names/scores
live in `TemplateFields`. The editor exposes the fields relevant to each TV
card, hides weather and exchange controls, and includes TV fields in preview,
clear and saved-layout loading. Draft values are excluded from QSettings and
layout JSON; only design/style/artwork/placement persist. Existing TX document
copy/equality includes these fields for atomic Apply and undo/redo. TV cards
produce ordinary flattened images/WAVs, so remote and receive protocols do not
change. Tests cover all designs/modes, active-field isolation, text geometry,
photo backgrounds, clearing/freshness, layout round trips and simulated TX/RX.


## Stable remote UI refresh (1.7.1)

`MainWindow::updateEnabled` computes final Play/Record/device/manual-start
states before setting them, avoiding enabled→disabled pulses on remote updates.
Radio badge and voice PTT CSS is set only when the desired style differs.
`RadioFrontend::setConfig`, its signal meter and `CaptionPreview::setTextRegions`
ignore unchanged content; real state, skin, image, region and z-order changes
still update through their existing paths. The main refresh is not delayed or
throttled, so PTT and busy-state changes retain their original timing.

`Server::setSnapshot` avoids sending identical snapshots. The client separately
ignores unchanged snapshot/reply state for compatibility with older hosts,
while continuing to process replies and audio packets. Heartbeats, PCM sequence
numbers, authentication and connection-loss handling are unchanged.

Regression probes count actual EnabledChange, StyleChange and Paint events
through an authenticated in-memory remote session with changing meter values.
They also verify preserved frequency edits, actual TX/RX transitions, latched
voice styling/release and command/audio delivery despite identical snapshots.


## Receive-status ownership (1.7.2)

Remote PCM must not publish the local automatic-capture status. That handler
used to insert the shack address and silence suffix, while updateReceive reset
the same label to a message without the address. It also alternated recordingInfo
between a local automatic-RX message and the remote device summary.

In client mode updateReceive owns the liveAudioState message; it formats a
short activity line and a fixed endpoint line. Full host audio errors stay in
the tooltip. updateRadioStatus owns the non-recording device summary, refreshed
when remote recording finishes. PCM only updates data/meters in automatic remote
RX. Recording elapsed text uses captureDevice so it identifies the actual shack
source. Local receive status behavior and the decoding/transport paths remain
unchanged. The regression interleaves actual PCM and changing station snapshots
and was verified to fail against the old 1.7.1 UI library before passing here.


## Fixed radio front panel (1.8.0)

`RadioFrontend` owns a fixed header, station strip, frequency/readback display,
scrolling control body, voice deck and status line. `addStationControls` and
`addVoiceControls` embed MainWindow's existing controls into the console instead
of placing separate panels above it. Their signal handlers and remote transport
remain unchanged. `addAudioMonitor` and `addAudioControls` register scroll targets;
section buttons align the requested panel to the top of the viewport, and
`showTuning` restores the top position.

The display reads filter width, power percentage and RIT directly from controller
state, separate from editable drafts, with unavailable placeholders after a
disconnect. `TuningDial` and `RadioSignalMeter` paint theme-aware instrument
surfaces. Meter updates still skip identical readings and badge/PTT styling
retains its equality guards. Stylesheets change with the selected skin, not on
each radio snapshot. Protocol, radio commands, audio paths and receive-status
ownership are unchanged.


## Station summary ownership and geometry (1.8.1)

`updateRadioStatus` owns `recordingInfo` whenever explicit recording is inactive,
including background local reception. It combines automatic-RX and radio/PTT
information instead of competing with `consumeCapture`. Recording PCM writes a
two-line elapsed/source message; finishing calls `updateRadioStatus` for every
station role. Equal normal summaries are left untouched. Tooltips preserve full
source and error text.

`recordingInfo` has a fixed two-line height. It and `liveAudioState` use an ignored
horizontal size policy so their text cannot increase the minimum width of
`operatingControlsBody`. This matters for unbroken ALSA identifiers: even a fixed
height, word-wrapped QLabel can otherwise widen the sidebar and make its device
fields change width when the message changes. Status changes do not alter capture,
decoding, CAT polling or remote transport timing.


## Laptop speaker monitoring (1.9.0)

`RemoteSpeaker` owns an independent 12 kHz mono S16_LE `aplay` process, a 20 ms
pump and a bounded jitter queue. It starts after 80 ms of PCM, trims excessive
backlog to about 200 ms, and re-primes after a 750 ms gap. It has no PTT or
microphone API and no voice-TX lifetime timeout. Output scaling applies only to
its private playback block. `VoicePlayer` retains its 48 kHz TX format, PTT gate,
watchdogs and three-minute limit unchanged.

`consumeCapture` forwards a copy of sample-aligned, receive-level-adjusted PCM
only in remote client mode with speaker playback active. The original chunk
continues to the waveform, recording and decoder unchanged. `updateRemoteListening`
uses the saved opt-in and current connection/RX/TX/setup state. Real state changes
start or stop the output; repeated snapshots do not restart it. Speaker errors
uncheck listening and display a separate status, without touching radio PTT or
network state. Closing stops playback before disconnect/teardown.

Microphone and speaker discovery run independent timed local ALSA listings.
Speaker selection commits on a combo activation or editingFinished, avoiding
reopening hardware on every typed character. Mic, speaker, volume and listen
preferences use separate voice settings; radio audio setup remains on the shack.


## Capture-only microphone check (1.10.0)

`VoiceCapture::levels` reports original sample peak/RMS, gain-scaled peak before
limiting, limiter activity and original full-scale clipping per 20 ms block.
`setGain` updates the digital gain live. Voice PCM limiting and the existing
48 kHz transmit format remain unchanged.

`MicCheckDialog` owns a separate VoiceCapture with no PCM sink, player, radio
controller or remote client. A bounded 1.2-second reading window drives native
meters and guidance at 10 Hz, keeping speech pauses from flashing between
categories. Clipping at the input takes precedence over output limiting and
peak guidance. Explicit done/close stops capture; a deferred initial start checks
visibility so a dialog closed before the first event cannot open hardware later.

MainWindow marks micCheckActive separately from remoteBusy so remote receive
continues. It gates TX/UI operations and local host transmit requests, mutes
RemoteSpeaker, and suspends local automatic capture while the dialog owns input.
Only an accepted dialog copies its device/gain into persisted voice settings.
Closing or Stop rejects the dialog, ends capture and restores receive/listening.


## Speaker gating during radio disconnect (1.10.1)

The Radio Disconnect action sets remoteRadioDisconnectPending before issuing the
command and calls updateRemoteListening immediately. The guard remains active
through intermediate receive snapshots. A successful reply already carries the
shack's disconnected radio snapshot, so clearing the guard cannot restart audio:
CAT-enabled speaker readiness also requires radioConnected. Audio-only stations
with radioEnabled false are exempt. A failed command unchecks listening before
refreshing controls; a new station configuration clears the pending guard.

The listening preference survives a successful disconnect and can resume on a
radio connection from either end. No CAT, capture, decoder or protocol behavior
is changed; the gate applies only to local speaker playback.


## Confirmed radio controls and TS-990S USB voice (1.11.0)

HamlibBackend maps public USB/LSB D1–D3 enum values to stable display names when
runtime name helpers omit them. Explicit TS-990S data modes parse through the
same mapping. Readback disables Hamlib cache immediately before each read;
rig_open's background initialization can otherwise restore cached requested
values. Controller::apply checks a separate fresh readback map after a successful
setter. Missing readback and mismatched values return errors, including aliases
PKTUSB/PKTLSB for D1. It never uses an old merged state to confirm a setting.

CAT PTT with a specific microphone/data input uses RIGMICDATA instead of RIG,
which can discard the input choice. Serial/GPIO PTT selection remains separate.
The TS-990S setup preset edits CAT/data selection and automatic PTT; save closes
the previous connection and explicit reconnect applies it. Voice PCM delivery,
click-to-lock behavior, mute gates and release watchdogs use the existing path.
No front-panel menu writes or automatic mode changes occur on PTT.

Remote displayedRadioConfig validates the host's existing configuration snapshot
before adapting connection status, so frontend and advanced controls use the
shack model. TS-990S mode choices include all six data profiles; Main/Sub and
RX Main / TX Sub split match the installed driver. Memory/VFO and bandwidth are
disabled for this backend; advanced rows also require valid readback. User mode
activation sends once; programmatic poll updates are signal-blocked. Typed
mode entries continue to use Set, preserving drafts across polls.

TS-990S tone-enable polling/set is excluded: the installed Hamlib function uses
unbanded TO, whereas Kenwood documents TO0/TO1. Mark the combined CTCSS control
unavailable instead of disconnecting CAT or claiming a tone was enabled. The
PTY fixture rejects unbanded TO and verifies that native operation never sends it.

A separate single-shot mode timer waits while the worker is busy. New user
activations replace the waiting command, and success preserves a newer selection
or typed draft. Stop, loss of operability and configuration changes discard the
pending mode so it cannot run later after transmission or reconnect.


## Embedded complete emoji catalog (1.12.0)

`emoji_catalog.qrc` embeds a generated JSON catalog with 3,953 unique Emoji 17.0
RGI strings, categories, CLDR names and keywords. A lazily initialized immutable
catalog parses it once and indexes exact strings plus preferred-tone families.
Only real same-tone RGI variants are used for the five preferred-tone choices.
Family lookup ignores variation selectors and skin modifiers only for its key;
inserted text always comes unchanged from a fully qualified catalog entry.
This avoids invalid selector placement or arbitrary tone insertion into a
joined sequence. Mixed-tone entries remain directly selectable in All skin tones.

CompositionDialog rebuilds only the filtered item list with paints suppressed,
then lays out batches of 120 items. Default/preferred-tone views omit explicit
variants, while index 6 exposes all exact catalog strings. Recent retains exact
forms and resolves their actual annotation rather than removing modifiers.
The saved preference accepts 0–6. A shared emoji-span matcher assigns Noto Color Emoji only to exact recognized
sequences. Qt 6.9+ text options disable the built-in emoji parser, whose table
predates some Emoji 17 wrestling sequences. Explicit fonts let HarfBuzz shape
the complete ligature. A native item delegate uses that option in the grid;
a syntax highlighter applies it in the plain-text editor without changing text
or undo history. The transmit composer applies the same formatting to its
QTextDocument, retaining ordinary caption fonts and its existing raster cache.

The developer generator uses ICU 78.2's public C UnicodeSet API and IBus's public
annotation reader through Python GI. These are maintenance dependencies only;
regular CMake builds and installed apps consume the committed embedded data.
The package requires fonts-noto-color-emoji >= 2.051 and includes Unicode notices.


## Radio profile store and application (1.13.0)

profiles::Settings and Store use typed, bounded JSON with immutable UUID identities,
unique names, an active ID and a catalog revision. A single radioProfiles/state
value stores up to 16 setups. The active payload is mirrored to legacy radio,
audio and rx keys in the same QSettings sync. The settings filename is captured
per MainWindow, keeping separate station instances and test hosts independent.
Startup preserves current effective legacy settings; invalid collections are
copied to a recovery key before creating Current radio from those settings.

Audio level and device/calibration edits schedule a 500 ms active-profile save.
Every explicit switch captures live outgoing settings before mutation. A candidate
store stays separate while CAT disconnects. The old store remains active on a
failed disconnect, session change or failed settings commit. Successful switches
apply all fields with signals blocked, reset RX gain/capture generations, close
model-specific advanced controls and resume normal receive. No connection, test
tone or PTT ON is requested. Metadata-only operations avoid RX suspension and
server preparation snapshots. Autosave changes payloads without changing the
catalog revision, so gain adjustments do not rebuild choices or invalidate edits.

The authenticated snapshot advertises bounded profile summaries; profiles.change
uses the expected catalog revision, a known ID and a validated operation/name.
Remote requests execute on the host, including transmitter reservation during an
actual switch. profileApplying participates in transmit and setup gates. Client
UI uses the host list while retaining its local store. The host TX percentage is
a non-persisting, read-only override in AudioLevelsPanel and clears when leaving
remote operation. Existing microphone/speaker and local monitor preferences are
independent. Profile identity accompanies CAT configuration edits, preventing a
stale dialog from writing settings onto another radio. Active-ID changes cancel
pending client encoding. Equality guards and QSignalBlocker keep normal snapshots
from rebuilding the selector or sending commands.

Radio connect/key/apply requests carry the active profile ID; releases remain
unconditional. Voice begin and SSTV upload begin/commit are also bound to the
selected profile, with the client retaining the upload's original identity.
The host rejects mismatches before invoking hardware callbacks. This covers
Wi-Fi packets queued before another operator changes profiles. Legacy clients
remain compatible with a single profile; multiple-profile stations require an
aware client for radio commands and transmissions.
