# Vector network analyzer workspace

Shared chirp-based forward-transmission workspace for
[ALPHA250](../../examples/alpha250/vector-network-analyzer/README.md) and
[Red Pitaya](../../examples/red-pitaya/vector-network-analyzer/README.md).
Include `components.mk` and provide a board `web/app.ts` that declares an
immutable `VNAProfile`, then calls `mountVNA()`. Both the page and acquisition
worker load the same compiled program and board profile.

## Shared components

| Location | Responsibility |
| --- | --- |
| `web/instrument/` | Instrument typography, controls, status and listener lifecycle |
| `web/plot-references/` | Reference collection, files, undo and sidebar controls |
| `web/plotting/` | Flot canvas renderer, logarithmic axes and selection |
| `web/vector-network-analyzer/` | Complex response model, workspace and acquisition worker |
| `processor/` | Freestanding WebAssembly reference reconstruction and real FFT |
| `server/drivers/vector-network-analyzer/` | Sweep planning, ownership and checked DMA RPC driver |
| `fpga/cores/chirp_generator_v1_0/` | Logarithmic phase generator, also used by `adc-dma-chirp` |
| `fpga/cores/vna_capture_v1_0/` | Taper, gain, aligned ADC capture and fault detection |
| `fpga/modules/vector-network-analyzer.tcl` | Shared DDS, FIFO, clock crossing and receive DMA assembly |

Board examples supply only the clock policy, converter wiring, memory map,
build configuration and browser profile. Red Pitaya does not import code from
an ALPHA250 example. New board profiles must agree with the server clock policy
and FPGA sample normalization. `VNAProtocol` defines the browser capture geometry;
changing it also requires updating the FPGA and server protocol.

## Measurement workflow

Connect the selected output directly to the selected input with the measurement
cables. Choose the span, point count, chirp duration and digital source level.
Select **Record thru → Acquire thru**, insert the device using the same cables
and settings, then select **Run sweep**. **Apply correction** divides the complex
response by the recorded thru. The other output stays muted; the source returns
to zero after each chirp.

This is thru-normalized forward transmission in the selected connections and
load. It provides S21-style measurements. A full two-port error model needs
reflection measurements and calibration standards. Source level is digital
dBFS; output voltage and power depend on frequency and load.

Calibration requires matching span, point count, duration, source level, record length,
frequency grid and hardware/demo source. A changed setup marks the thru stale.
Restore its settings or record another thru. Cable, analog and clock changes
also require a new thru. ADC near-rail samples reject the entire sweep; this
check does not detect every form of analog distortion.

## Workspace

The compact header, toolbar, sidebar, controls and references use the same SDK
components as FFT and phase-noise instruments. The VNA plot adapter maps complex
responses to magnitude, phase and delay, reusing Flot instances across updates.
Sweep labels sit beside their fields in one compact desktop toolbar, using the
same 26 px controls and muted accent as FFT. Plot actions sit beside magnitude;
phase/delay and unwrap controls sit beside the secondary plot. Progress and
notices share the desktop footer, while smaller screens keep them near the
acquisition controls. Desktop plots fit the available height.
The desktop sidebar scrolls independently, so opening notes and preferences
keeps plot sizes and axes stationary.
The status area and Stop button keep their space between sweeps. Readouts and
marker columns keep fixed geometry, and the last completed result and timing
remain visible while the next sweep runs.
Y limits and ticks stay fixed while repeated traces fit the current range. A
trace crossing a limit expands only that edge, with padding. Reset refits both
axes; zoom, display/correction settings and visible reference changes also refit
the affected view. Wrapped phase keeps its fixed −180°–180° scale.
While dragging a frequency selection, both plots keep the same response and
resume with the newest completed sweep on release. Hover readings follow new
results at the same cursor frequency, show phase or delay for the selected view,
and clear when the pointer leaves the plot.

- Drag either plot to zoom both frequency axes. Double-click, **Reset**, or
  Escape on a focused plot restores the full span.
- Edit three markers using frequencies such as `10 MHz`. Readouts use the nearest
  acquired point; the tooltip reports its frequency. Clicking a plot positions
  M1. **Find peak** and **Find −3 dB** place markers at acquired points.
- **Group delay** differentiates unwrapped phase. Phase changes between adjacent
  points must be resolved; increase the point count for long delays. Deep
  stopbands limit phase and delay accuracy.
- Positive **Port extension** removes delay from displayed responses while
  retaining acquired and calibrated complex data.
- **Capture ref** stores up to eight responses using the shared reference panel.
  Rename, hide, recapture, remove and undo captures; save/load reference files.
  Each capture retains its grid, settings and correction at capture time.
- **Repeat** starts another complete sweep after a 16 ms UI update interval. **Stop** cancels
  acquisition or processing and disables repetition. Completed data remains
  available after a cancellation or error.
- Browser storage preserves settings, response, thru and references. **Session
  JSON** backs up that state. Loading a session changes no hardware output.
  Existing V1 sessions and earlier four-trace files continue to load.
- **CSV** includes displayed complex values, magnitude, phase, delay and
  correction metadata. **PNG** includes both plots, overlays, the current zoom
  and acquisition metadata, wrapping long reference names.

`?demo=1` runs labelled synthetic data without hardware commands. Board profiles,
hardware/demo sessions, calibrations and reference files stay separate.

## Processing

**Auto** records the chirp plus at least 10 ms of response, rounded up to a
power of two (4–64 Mi samples). **Full** records 64 Mi samples for longer ringing
or finer FFT spacing. A shorter record can truncate a device's response; select
Full and compare when measuring narrow resonances or long delays. The logarithmic
grid snaps to the selected record's FFT bins, so some requested points may merge.
Changing record length requires a matching thru. Older sessions restore Full
and retain their original grid, calibration and saved traces.

| Board / chirp | Auto samples | Download | Record duration | FFT spacing |
| --- | ---: | ---: | ---: | ---: |
| Red Pitaya / 100 ms | 16 Mi | 32 MiB | 134.2 ms | 7.45 Hz |
| Red Pitaya / 250 ms | 32 Mi | 64 MiB | 268.4 ms | 3.73 Hz |
| ALPHA250 / 100 ms | 32 Mi | 64 MiB | 134.2 ms | 7.45 Hz |
| ALPHA250 / 250 ms | 64 Mi | 128 MiB | 268.4 ms | 3.73 Hz |

A persistent browser worker downloads up to sixteen checked 256 KiB DMA packets
per RPC (4 MiB), releases board ownership, reconstructs the exact FPGA source
from its coefficient, seeds, sample count and gain, and computes
`FFT(ADC) / FFT(reference)`. The board profile supplies the DAC quantization.
There is no extra record window or arbitrary delay removal. Low-excitation
bins become missing values. Repeated sweeps reuse the compact reference spectrum
only when the actual FPGA plan, record length, sample rate and source quantization
match and every requested bin is cached. The cache holds the 2001-point grid
and the current requested grid, so point-count presets can share a reference;
custom grids rebuild it when needed. A cancelled partial reference is never cached. The
worker yields through a message channel so cancellation stays responsive without
nested timer delays. The packed real FFT combines pairs of radix-2 stages to
reduce memory traffic while keeping double-precision twiddles and float
intermediate rounding. ADC conversion, FFT ordering and the first stage pair
run together; early stages finish in 2 KiB blocks before traversing the full
record. This reduces memory traffic without changing the acquired record or
FFT grid. The workspace updates completed responses through the shared Flot
data redraw path when the axes, legend and size are unchanged. This preserves
grid/text caches and canvases. It retains the previous plot when starting the
next sweep and defers both panels together during selection.
The readout reports total time and downloaded bytes; its
tooltip gives capture, download and processing times.

The packed real FFT uses approximately six bytes per sample: 96 MiB at 16 Mi
samples, 192 MiB at 32 Mi, or 384 MiB at 64 Mi, including raw ADC storage.
The worker retains its largest allocated workspace until the page closes.
Allow at least 1 GiB of free browser/host memory for a full sweep. A desktop
browser with WebAssembly and workers is required. Downloads and processing add
to the hardware capture duration.

The first web build creates an LLVM 18 Docker builder from
`web/vector-network-analyzer/processor/Dockerfile`. Subsequent builds reuse it.
`VNA_WASM_IMAGE` and `VNA_WASM_CC` can select a different builder/compiler.

## Checks

From the SDK root:

```sh
VIVADO_VERSION=2026.1 bash web/vector-network-analyzer/tests/run.sh --red-pitaya
source /tools/Xilinx/2026.1/Vivado/settings64.sh
vivado -mode batch -nolog -nojournal -source fpga/cores/vna_capture_v1_0/tests/run_sim.tcl
vivado -mode batch -nolog -nojournal -source examples/alpha250/adc-dma-chirp/tests/run_sim.tcl
```

The runner builds both browser profiles, checks the C++ planner and runs the
numerical/model tests: independent DFT, exact integer reference for both DAC
widths, known FIR/delay, full-record FFT, calibration, missing values and session
round trips. It also checks shared socket IDs and concurrent reads with
out-of-order connections/replies. `tests/test_browser.py` uses Python Playwright for real Chromium
checks; add `--red-pitaya` for the second profile. `tests/emulator.cjs` uses Node's
`ws` package and production driver metadata, binding only to localhost. Run
`test_browser.py --protocol` in its network namespace for a full 128 MiB protocol
fixture, adaptive record sizes, cache reuse/invalidation, gain/delay recovery,
malformed download rejection, clipping and cancellation followed by reuse.
`tests/test_layout.py` checks workspace positions on every animation frame
during first results, repeated sweeps, cancellation, validation and hover at
320–1440 pixels. Add `--red-pitaya` for that board's profile, or `--protocol`
with the emulator to cover the acquisition worker's progress stages.
`tests/test_axes.py` checks repeated gain/phase changes through real Flot plots,
including stable limits, ticks and grid pixels, expansion, missing values and
refitting after reset, view, extension, reference and zoom changes. It also checks
that steady sweeps avoid grid rebuilds, that both panels defer updates during
selection and resume on release, and that stationary hover readings stay current. Add
`--red-pitaya` for that board's profile.
Run `node web/vector-network-analyzer/tests/benchmark_processor.cjs` from the
SDK root in a Node development environment to compare paired, cache-blocked
and fused ADC processing on identical 16, 32 and 64 Mi-sample records. The default processor
is the ALPHA250 build; an optional first argument selects another Wasm file.

These checks do not connect to hardware. Board READMEs report build checks and
physical acquisition tests separately. RF accuracy requires known thru,
attenuator/filter and cable-delay comparisons against an independent instrument.
