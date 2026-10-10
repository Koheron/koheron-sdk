# ALPHA250 DPLL

Two independent digital phase-locked loops with four integrators, signed gains,
DDS reference oscillators and selectable DAC routing. The current design runs at
250 MS/s. Use a V1 OS image.

```sh
make CFG=examples/alpha250/dpll/config.mk validate
make -j CFG=examples/alpha250/dpll/config.mk
make CFG=examples/alpha250/dpll/config.mk HOST=192.168.1.100 run
```

`run` streams logs; Ctrl+C stops the stream and leaves the instrument running.

## Web controls

Both ADC loops are visible together. Frequency editors share the FFT/PNA digit
controls: click a digit, tune with arrow keys or the wheel, or type a frequency
with optional units. Enter or leaving the field applies a typed value; Escape
restores the latest readback. Changing the unit alone changes the display.
The allowed range is 0 through half the actual DAC sample rate, inclusive.

Gains show polarity separately from magnitude. The magnitude column defaults to
**dB** (`20 log10 |g|`, relative to coefficient 1); its header switches to **log₂**
for base-2 exponent editing. Values use 16 steps per octave (about 0.376 dB).
Typed dB values round to the nearest hardware step. Coefficient approximation
error is at most 0.020373%. Sign 0 disables the gain and displays **Off**; 0 dB
with either nonzero sign means magnitude 1. Re-enabling restores the last applied
magnitude. Unit changes do not send commands or apply pending edits.
Press Apply or Enter to send a gain, Escape to cancel a draft. Arrow keys tune one
step; Shift + arrow tunes one octave. Drafts survive polling and stay with their
ADC channel. The positive exponent limit is 30.9375 (186.26 dB); negative gains
also allow 31 (186.64 dB).
Integrator checkboxes, DAC routes and the 10 MHz clock reference apply directly.

Startup reads existing settings without changing them. Connection failures
disable controls and expose Retry. Closing the page stops polling and cancels
queued frequency edits. The collapsible signal diagram follows applied readback for its selected channel,
including signed dB gains, enabled integrators, mode, reference and routing.
Focusing a loop control selects that channel in the diagram. Matching gains,
integrators, reference frequency, mode and routing controls highlight together.
Click a DDS, gain, integrator, mode selector or RF routing block (or press Enter/Space when focused)
to open its existing controls beside the block. Opening an editor never changes
a setting or expands a collapsed loop panel. Repeated Int 3 blocks share the
same enable control. The DDS block shows frequency to 1 Hz; its tooltip retains
finer readback precision. DDS edits use the standard digit control: Enter or
leaving the editor applies typed entry, Escape cancels it, and changing display
units alone sends no command. Disconnect cancels an unfinished frequency edit.
Tab and Shift+Tab leave an editor in the diagram's normal keyboard order.

The gain editor shares the exact draft and Apply action with the standard table.
Closing retains its draft; Escape cancels it, and a successful Apply closes after
readback. Integrator enables, mode selection and routing apply directly, as in
the standard controls; closing their editor leaves applied settings in place.
The original control's space shows applied values while editing, preserving the
layout. On wide screens both loops sit beside the diagram; compact screens
retain the stacked layout. The diagram keeps applied values while a gain editor
contains a draft. Its geometry stays fixed, and motion follows the browser's
reduced-motion preference.

## Manual Accurate / Fast P + I

Each channel defaults to Accurate. Acquire lock, then select **Fast · near lock**
manually. Fast replaces the direct P and I contributions with a calibrated local
phase estimate. I²/I³ remain on the accurate estimator and retain their states.
Both sources run continuously; automatic switching is not implemented.

Each ADC/DDS pair has one 24-bit complex mixer, split before filtering:

```text
                         → four-sample boxcar → calibrated projection → P + I
ADC + DDS → 24-bit mixer
                         → four 16-sample moving sums → custom 24-bit extractor → unwrap
                                                                      ├→ accurate controller
                                                                      └→ monitor selector
                                                                           → epoch origin → CIC/FIR/DMA
```

The accurate filter retains full intermediate sums and has 120 ns group delay
plus five pipeline clocks (20 ns) at 250 MHz. The fast boxcar retains its two
pipeline clocks and 6 ns group delay. Sharing the accurate extractor eliminates
the dedicated monitor CORDIC. The accurate branch selects PR 780's
15-clock, 24-bit custom extractor and canonical fused unwrapping; its filter
and the Fast P + I controller retain PR 782's architecture. A vendor CORDIC
remains selectable in `split_detector::create` as a regression reference. Monitor resets change its downstream origin;
feedback phase history and controller states are independent of monitor epochs,
channel selection, decimation, precision and backpressure.
The continuous 64-bit monitor history uses two 32-bit words and is delayed by
one further clock (4 ns). Feedback phase reconstruction uses the frequency bus
directly and retains the delays below. Canonical unwrapping registers the raw
modular difference and the positive-pi boundary flag together, then corrects the
sign after that same register edge. This shortens the subtraction path while
preserving signed pi ties and sample timing.

The accurate phase remains in Q8 legacy phase units through unwrapping and into
both initial gain tables: frequency is signed 25-bit and feedback phase is
signed 40-bit. Their output shifts absorb the eight extra fractional bits;
legacy gain values and accumulator units are retained. The monitor rounds the
continuous 64-bit Q8 phase history into its existing phase unit only at its
consumer interface, with an independent rounding stream.

Entry captures a coherent 64-sample average of fast-filtered I/Q and accurate
feedback phase. Phase averaging uses modular differences around its first sample
so crossing the feedback phase wrap does not corrupt the reference. Two further
clocks drain the registered phase averaging arithmetic before acknowledgement;
these clocks affect manual calibration only. Software
programs signed Q6.18 projection coefficients:

```text
S = 8192/pi
cx = -Q0*S/(I0²+Q0²)
cy =  I0*S/(I0²+Q0²)
local_phase = I*cx + Q*cy = S*(A/A0)*sin(phi-phi0)
fast_I_phase = captured_accurate_phase + local_phase
```

The fast I reference therefore retains the actual accurate error rather than
redefining the captured lock phase as zero. The minimum reference amplitude is
64 filtered I/Q counts. Amplitude changes scale detector gain by A/A0; the
sine approximation has 0.51% error at ±10 degrees for stable amplitude. The
range diagnostic reports roughly ±14 degrees and at least half the captured
amplitude. It is not lock detection and never switches modes automatically.
Image rejection of the short boxcar depends on carrier frequency. The generated
frontend test covers 31.25 MHz; other carriers need characterization before Fast
is used in feedback.

The fast I accumulator follows the accurate direct I state while Accurate is
selected, then integrates fast phase in Fast mode. The always-running accurate
controller separately tracks direct P, direct I and I² contributions; their
modular sum equals its full RF correction. Its nested higher-order states never
consume fast phase. The selector first combines fast I and accurate I² in a
register, then adds fast P and the handoff offset in the output register. This
adds no clock to P. Entry waits sixteen clocks for calibration to propagate;
each handoff holds one sample and captures a constant offset to preserve
continuity.

Fast requires phase reconstruction and RF accumulation (API indices 0 and 2,
labelled integrators 1 and 3 in the UI). Disabling either or changing DDS
frequency returns to Accurate. After changing the signal or reference, reacquire
and select Fast again.

The latest V1 RPC IDs 0–11 are retained. Appended calls are `set_p_mode`
(ID 12, mode 0=Accurate/1=Fast) and `get_p_path_status` (ID 13). These legacy
prototype names now control P and I together. Mode selection returns 0 after
hardware acknowledgement, -1 for invalid input, -2 on timeout, or -3 for
disabled required integrators / inadequate reference amplitude.

`tests/run-p-path.sh` checks projection arithmetic, coherent captures, both AXI
channels, jump-free handoffs, independent accurate states, fast I response,
fractional gain scaling, modular phase reference capture, monitor reset isolation
and the generated production frontend. See the build section for full timing.
The pulse-valid test measures four mixer clocks and 15 clocks for the selected
24-bit custom extractor. Register delays at 250 MHz are:

| Stage | Fast P | Fast I | Accurate P | Accurate I |
| --- | ---: | ---: | ---: | ---: |
| Shared mixer | 4 | 4 | 4 | 4 |
| Prefilter pipeline | 2 | 2 | 5 | 5 |
| Local projection | 3 | 4 | — | — |
| Custom phase extraction | — | — | 15 | 15 |
| Difference and unwrap | — | — | 1 | 1 |
| Phase reconstruction | — | — | — | 1 |
| Captured reference addition | — | 2 | — | — |
| Gain | 3 | 4 | 4 | 4 |
| First controller sum | — | — | 1 | 1 |
| RF accumulation | — | 1 | 1 | 1 |
| I + I² preparation | — | 1 | — | — |
| Mode selector | 1 | 1 | 1 | 1 |
| RF DAC mux | 2 | 2 | 2 | 2 |
| **Total clocks / time** | **15 / 60 ns** | **21 / 84 ns** | **34 / 136 ns** | **35 / 140 ns** |

From filtered I/Q through the selector, Fast P is seven clocks (28 ns) and
Fast I is thirteen (52 ns). The accurate I² branch adds five clocks (20 ns) after
the corresponding direct accurate contribution through the first accumulator
and I² gain. In Fast mode, its output also passes through the I + I² preparation
register. All RF DAC modes include one additional 4 ns output register after
the source-selection register. The first register can remain near the loop
logic while the final register sits near the DAC pins. I³ has its own gain
and accumulator before the precision DAC;
that DAC's serial transfer and settling are separate from RF feedback timing.

These are register delays. Add 6 ns filter group delay for Fast, or 120 ns for
Accurate, when budgeting small-signal phase delay. Converter pipelines,
ADC/DAC interface timing and the analog plant are also separate. The DDS runs
in parallel and its reference-generation latency is not added to the ADC
disturbance path.
Hardware lock, noise, loop stability and analog latency still require board tests.

## Continuous phase-noise monitor

The spectrum panel uses the standard PNA implementation: channel selection,
CIC decimation, 1–100 rolling averages, phase/frequency noise, smoothing,
zoom/Fit, frozen reference traces, CSV/PNG export, carrier power and integrated
phase/time jitter with its actual integration band. Precision is selectable
from standard through eight extra fractional bits. The reference is the
selected loop's existing DDS frequency; set it close to the input carrier.
Coverage and queue indicators use the shared PNA status widget, reporting
skipped sample coverage and whether processing keeps up with incoming windows.

The monitor consumes the selected loop's shared accurate phase history after
its own epoch origin. The shared `pna_single_stream.tcl` retains the full-precision
six-stage fixed CIC /2 at 250 MHz, a crossing to 143 MHz, six-stage programmable
CIC /(R/2), 40-bit FIR /2, packet quantization and cyclic SG DMA. These stages
remain outside feedback. Changes to monitor logic require routed timing checks
because placement and routing are shared with the controllers.

Monitor resets, channel, decimation and precision changes do not reset feedback
accumulators, change gains, switch DAC routes or retune a reference. Monitoring
continues with feedback integrators disabled. Loop edits invalidate monitor
averages and start a new acquisition epoch.

The server uses the shared PNA `Core`, cyclic DMA reader, 32768-point Hann FFTs
with 50% overlap, three-periodogram Welch estimate, rolling averager, calibration
and atomic spectrum snapshots. The web UI uses the shared PNA driver adapter,
passive monitor lifecycle, plot, precision widget, numeric editors and exports.
The local `DpllMonitor` adapter supplies a separate ordered socket and the `Dma`
driver to `PnaMonitor`, keeping monitor replies out of the feedback-control
command queue. The clock RPC adapter is also shared with the ALPHA PNA pages;
DPLL retains its own clock template and feedback-control lifecycle.

DMA acquisition continues while the CPU computes spectra or the browser is
closed. Slow consumers can skip FFT windows; the existing PNA status reports
these skips and preserves valid averages. Damaged/overrange packets are rejected
and restart only the monitor. At the default R=20 the filtered sample rate is
6.25 MS/s; at R=8192 it is about 15.259 kS/s.
Total CIC rates are even integers from 4 through 8192. Both halves use six
stages, so their cascade has the same CIC response and gain as one six-stage
CIC at rate R. The fixed stage retains all six extra bits; the slow stage uses
110-bit modular arithmetic and the existing PNA power-of-two normalization.
The shared implementation is in [`pna_filter.tcl`](../../../fpga/lib/pna_filter.tcl).

The monitor uses the same acquisition buffers and DMA mode as the PNA designs:
512 cyclic SG DMA packets of 8192 samples, copied into a 65536-sample processing
window. The server continuously advances by 16384 samples (50% FFT overlap)
and forms its three-periodogram Welch estimate using 32768-point FFTs. There
is no separate raw-capture buffer or DMA reader.

The original `Dma` command IDs 0–3 are retained, but **`get_data()` now returns
65536 float32 phase values in radians**, matching PNA `get_phase()`, instead
of one million int32 raw counts. CIC/FIR gain and precision correction are
already applied; custom clients must update their reply length/type and remove
any raw-count conversion. `get_data_size()` reports 65536. `get_phase()` is
also available under the shared PNA name.

These calls return the latest processed phase window without waiting for or
restarting DMA. For validity, precision and a sequence number in the same reply,
use `get_phase_snapshot()`; a repeated sequence means no new window is available.
Use `get_spectrum_snapshot()` for the server's continuously averaged spectrum.
Loop edits immediately make old phase, spectra and jitter unavailable while the
worker starts a new acquisition epoch. Raw `DmaS2MM.start_transfer` remains
disabled on SG hardware to protect the continuous ring.

`test_time.py` displays the shared server spectrum with a read-only Python client:

```sh
python examples/alpha250/dpll/test_time.py 192.168.1.100
```

## Host checks

```sh
make CFG=examples/alpha250/dpll/config.mk web server drivers_json
bash examples/alpha250/dpll/tests/run-host.sh
bash examples/alpha250/dpll/tests/run-p-path.sh
bash examples/alpha250/dpll/tests/run-dac-mux.sh
bash examples/alpha250/dpll/tests/run-monitor.sh
```

The host script requires g++-13, Eigen, NumPy/SciPy, and `typescript` and `jsdom`
(listed in `web/package.json`, install there with `npm install`). Set `NODE_PATH`
if they are installed elsewhere. It tests coefficient generation and the
acknowledged programming protocol with address/undefined-behavior sanitizers.
It delegates to the [shared host runner](../../../server/drivers/phase-noise/tests/README.md)
and also covers monitor acquisition and Python clients without Vivado. Set
`PNA_TEST_MODE=docker` to use the SDK test images. The historical detector and
integer-gain references live in `tests/reference/`; production feedback wiring
checks instantiate `split_detector::create`.
The web tests mount the actual templates, driver adapters and shared
frequency editor with a simulated transport. They check read-only startup,
channel isolation, signed/zero gain validation, frequency units and limits,
clock/routing commands, failure handling and disposal.

These are build and host checks. They do not validate board lock performance,
loop stability or FPGA timing. `run-monitor.sh` also simulates coherent status,
reset and metadata transfer between the 250 MHz and 143 MHz domains. FPGA
feedback simulation checks remain in `tests/run-fpga.sh`.

## Gain implementation and APIs

Both controllers use double-buffered lookup tables prepared when a gain changes.
The fast P gain uses registered DSP inputs, two parallel products and a small
final sum. It takes three clocks, receiving the combinational projection one
clock before the I/status phase register, so total P latency is retained. The
existing double-bank programming protocol is retained. It captures the gain
from the unsigned address-one entry. The wider I gain and accurate controller
gains use four-clock table reductions. Native carry chains compute the carry
from all discarded lower bits at the third register boundary; the last stage
adds only the retained output bits. The I² final sum is 32 bits. Feedback phase
and the accurate first and second integrators use DSP accumulators with their
existing one-clock updates, adding six DSPs across the two channels. All paths
accept one sample per clock. The sole additional RF feedback clock is in the
DAC output mux, selected only for DPLL; the shared mux defaults to one clock.
The shared accurate extractor uses the wider phase interface described above.
Earlier controller measurements used narrower two-clock P/PI tables.
See the [arithmetic and latency measurements](tests/gain_latency/README.md).

The existing `set_p_gain`, `set_pi_gain`, `set_i2_gain` and `set_i3_gain` RPCs
retain their command IDs and exact signed 32-bit integer gains. New RPCs are
appended:

- `set_geometric_gain(channel, gain, sign, step)`: channel 0/1; gain 0=P,
  1=PI, 2=I2, 3=I3; sign -1/0/+1; step is the integer exponent multiplied by 16.
  Steps 0..495 support either polarity; step 496 supports negative or zero only.
  Returns 0 after commit, -1 for invalid arguments, -2 for a programming failure.
- `get_gain_values()`: exact applied gains in the order P0/P1, PI0/PI1,
  I2-0/I2-1, I3-0/I3-1. A failed acknowledgement returns NaNs and disables the UI.
  The legacy `get_control_parameters()` layout is unchanged and rounds fractional
  gains to the nearest integer. Use the new readback with geometric gains.

The old gain control-register offsets are retained as legacy RPC shadows;
direct register writes no longer program gains. The new control registers are
`gain_table_command` and `gain_table_data[2]`. Status registers expose the
acknowledgement, active banks and eight signed 64-bit Q*.11 coefficients.
The driver writes both data words, changes the command toggle, and waits for
acknowledgement before reusing the port. Hardware issues a single RAM write and
rejects active-bank writes. A commit switches banks and records the coefficient
on the same clock. RPC serialization protects this shared programming port.
Server restarts read the active banks and coefficients from hardware.
Gain requests are validated and decoded at 143 MHz, using PR 780's registered
CDC handshakes and reset draining. RAM writes and atomic bank/coefficient commits
remain at 250 MHz. Address and payload precede the registered write strobe,
with a setup wait giving three clocks (12 ns) before RAM capture. Acknowledgement follows the write
or commit. Acceptance and the complete one-hot commit enable are decoded on the
existing acceptance register edge. The following edge atomically applies each
bank and coefficient with only reset gating its enable, avoiding a shared
state/rejection decode on the wide coefficient hold path. Programming and
feedback latency are unchanged. Reset preserves committed gains while cancelling
pending transfers, including a commit cancelled on its apply edge.
Only these held RAM programming inputs use three-clock timing constraints; the
write strobe, bank commits, lookup addresses and feedback remain at 250 MHz.

The AXI/controller integration simulation runs two controllers against independent
signed-product/state models while programming gains, including reset, rejected
writes and held commands with changing data:

```sh
export DPLL_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh
bash examples/alpha250/dpll/tests/run-table-system.sh
bash examples/alpha250/dpll/tests/run-gain-programmer.sh
```

The programmer reset test sweeps 65 write reset positions and 65 commit reset
positions for each of the eight destinations, plus a directed reset on each
destination's commit edge. It checks preservation of committed
gains, cancellation of in-flight transfers, rejected writes/commits and recovery.

## Full FPGA build

```sh
make -j4 CFG=examples/alpha250/dpll/config.mk all
make CFG=examples/alpha250/dpll/config.mk timing
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250/dpll/tests/check_table_design.tcl \
  -tclargs tmp/examples/alpha250/dpll/fpga/dpll.xpr \
  tmp/tests/alpha250-dpll/full-design
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250/dpll/tests/check_monitor_design.tcl \
  -tclargs tmp/examples/alpha250/dpll/fpga/dpll.xpr
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250/dpll/tests/test_monitor_stream.tcl \
  -tclargs tmp/examples/alpha250/dpll/fpga/dpll.xpr
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250/dpll/tests/test_split_cic.tcl \
  -tclargs tmp/examples/alpha250/dpll/fpga/dpll.xpr
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250/dpll/tests/test_extractor_latency.tcl \
  -tclargs tmp/examples/alpha250/dpll/fpga/dpll.xpr
```

The normal build enforces routed setup, hold, pulse-width and bus-skew checks
before writing the bitstream. The additional design check verifies the full
instrument top, 250 MHz clocks, both selected controllers and all eight table
gain paths. Physical optimization adds no pipeline stages and retains the
startup clock-phase timing constraints.
The extractor's final register follows scale selection and zero handling, so
unwrapping starts from a registered phase word without adding a feedback clock.
Gain acceptance has its own programming stage. The former vendor-CORDIC
netlist's fixed register placements are removed so the combined instrument can
be placed and qualified afresh.
The monitor stream simulation uses the shared CIC RTL and imports the production
clock converter and FIR configurations. It checks ordering, sustained throughput at R=4/20/8192,
sample-gap reporting under backpressure and recovery after an epoch reset.
Build results and hardware measurements are reported separately in the
[latency notes](tests/gain_latency/README.md#integration-and-hardware-status).

### Phase/gain retiming integration on V1 (2026-10-10)

PR #838 was rebased onto V1 `d2d35ba9`, including the shared lookahead
accumulator from #834 and residual lookup from #837. The initialization
conflict preserves the accumulator's phase state and the canonical difference's
registered raw value and pi-tie flag. No pipeline clocks or constraints changed.

The shared seven-suite unwrapper/history regression passes, including all twelve
latency configurations and canonical differences with both lookahead and split
history. The DPLL unwrapper, gain-programmer reset, two-controller programming
and P/PI/front-end suites also pass with unchanged sample latency.

The fresh full build **fails setup timing** on this integrated revision:

```sh
make -j4 CFG=examples/alpha250/dpll/config.mk \
  TMP=tmp/pr838-v1-integration VIVADO_VERSION=2026.1 MODE=development \
  N_CPUS=4 ENFORCE_TIMING=1 fpga
```

Worst setup slack is **-0.003040 ns**, with **-0.006079 ns** total negative
slack. The failing path runs from the gain-programmer state register to the
replicated channel-1 command enables. Hold (+0.042 ns), pulse width and all
12 bus-skew checks pass. The strict gate blocks bitstream generation; this
revision is **not qualified for merging**. The earlier passing result on the
old PR base does not qualify this combination. No further optimization trials
were run, and no hardware was deployed or tested.

### Residual normalization build (2026-10-10)

The extractor now registers the normalized residual coordinate on the existing
final CORDIC edge. This replaces three reciprocal ROM reads and a late scale
mux with one read, preserving the 15-clock extraction latency and output bits.
Both DPLL channels use the change; the standard PNA vendor CORDIC is unaffected.

```sh
make -j4 CFG=examples/alpha250/dpll/config.mk \
  TMP=tmp/rtl-residual-normalized VIVADO_VERSION=2026.1 MODE=development \
  N_CPUS=4 ENFORCE_TIMING=1 fpga
```

This fresh full build produces `dpll.bit`. At the unchanged 250 MHz clock,
setup slack improves from **-0.077903 ns** to **+0.014889 ns**; hold slack is
**+0.041407 ns**, with zero total negative slack/pulse-width violation and all
**12 bus-skew checks** passing. No constraints, implementation directives or
pipeline depths were changed. Setup margin remains small and must be checked
again after further edits. Synthesis removes **422 LUTs and eight registers**;
the final routed design uses 21,922 LUTs, 30,987 registers, 101 DSPs and 36 BRAM
tiles.

The full `check_table_design.tcl` passes, including both extractors' two-DSP
structures/register settings, controller paths and gain-programming constraints.
Paths through the extractors have +0.187/+0.021 ns setup slack. Standalone
arithmetic, before/after bit equivalence and production mixer/extractor latency
simulations pass; details are in the
[phase-extraction notes](tests/phase_extraction/README.md#registered-residual-normalization-2026-10-10).

**Hardware tests:** none for this revision. Existing board constraints still
leave 14 inputs and 41 outputs without I/O delays; the passing build qualifies
the currently constrained paths.

### Combined PR 780/782 build (2026-10-07)

The complete ALPHA250 instrument builds at 250 MHz with Vivado 2025.1.
Strict timing enforcement passes before writing `dpll.bit` and packaging
`dpll.zip`: setup slack **+0.006867 ns**, hold slack **+0.024832 ns**, zero total
negative slack and all **10 bus-skew constraints** passing. Setup margin is
small; subsequent logic or placement changes require qualification again.
The full design was synthesized and placed afresh. After adding the guarded
reference-launch routing hook, post-route optimization was rerun from the same
route checkpoint, followed by the normal strict build and packaging steps.
The routing hook checks identical data, clock, enable, reset and initialization
before using an existing projection register for two reference-carry inputs.
It adds no latency.

The final critical path is programming-state control to an applied-bank
register's enable, with a 4 ns requirement. Held programming address/data
inputs have a 12 ns requirement and +3.405 ns setup margin; the write strobe
and bank commits retain 4 ns requirements. Routed utilization is **22,338 LUTs**,
**31,089 flip-flops**, **101 DSPs** and **36.5 BRAM tiles**.

The single additional feedback clock is the RF DAC output register: Fast P/I
and accurate P/I register delays are **60/84/136/140 ns**, respectively.
All accurate gains retain four-clock latency. The generated configuration and
both controllers' extractor, gain, detector and selector timing checks pass.
Explicit DAC checks pass at startup phase 0 (setup +1.022 ns, hold +0.090 ns)
and the normal 56-step phase (setup +0.022 ns, hold +1.090 ns).

The DAC mux regression covers both its unchanged one-clock default and the
DPLL two-clock selection, each for 10,002 cycles. Independent four-clock gain
vectors pass 55,408 cycles; controller arithmetic passes 33,890 cycles, mapped
DSP controllers pass 19,548 cycles, and the two-controller programming test
passes 200,452 cycles with 4,352 transactions, 4,096 writes and 128 commits.
The production frontend, phase/history, host/web and monitor regressions pass.
The combined instrument was installed on ALPHA250 `192.168.1.13` on
2026-10-07. Chrome tests with DAC0→ADC0 and DAC1→ADC1 verified both
DDS loopbacks, monitor acquisition, gain readback, and manual Fast P+I
calibration. A +1 kHz input/reference offset on ADC0 measured
−999.999982 Hz phase slope. Both Fast paths reported within range at
31.25 MHz. These tests do not establish closed-loop stability or analog latency.

The hardware test exposed two UI defects, now fixed: an expected Fast-mode
calibration rejection disconnected the ES5 build, and a valid all-zero
spectrum was misleadingly labelled as settling. The UI now retains the
connection, identifies integrators 1 and 3, and visibly recommends greater
phase precision when no noise is resolved. At decimation 200 and +8-bit
precision the tested monitor achieved 100% coverage; decimation 50 exceeded
the measured processing capacity. Original settings were restored after testing.
Follow-up Chrome tests verified CSV (including live and reference spectra) and
PNG exports, eight-spectrum averaging, invalid frequency/decimation rejection,
1 Hz digit tuning, and negative fractional gain readback. Escape now cancels
gain drafts from sign buttons as well as the exponent input. Gain editors retain the last nonzero
exponent while zeroed within the current page. Arrow keys tune by 1/16 octave;
Shift+Arrow keys tune by one octave. Changes still require Enter or Apply. Live/empty plot
states retain identical plot and control bounds; no extra feedback panels were
added. Original settings were restored after these checks.

External I/O timing coverage remains at the board constraints' existing 14 inputs and 41 outputs
without delay constraints; lock, stability, phase noise and analog latency
still require hardware measurements.

### Historical PR 782 Fast P + I build (2026-10-07)

Before integrating PR 780, the ALPHA250 instrument built with Vivado 2025.1 at
250 MHz, including
the shared accurate extractor, manual Fast P + I controller and monitor.
Synthesis, placement and routing completed; after the placement-only hook change,
the final physical optimization step was rerun using the existing synthesis and
route checkpoints. Strict timing enforcement passed before writing `dpll.bit`
and packaging `dpll.zip`: setup slack **+0.004494 ns**, hold slack **+0.026840 ns**,
zero total negative slack and all **nine bus-skew constraints** passing.
This leaves little setup margin; subsequent logic or placement changes require
the full timing checks again.

Routed utilization is **24,147 LUTs**, **33,930 flip-flops**, **91 DSPs** and
**36.5 BRAM tiles**. Both controllers' accurate and fast gain paths pass their
individual timing checks. The held gain-programming address/data fields have
an 8 ns requirement, while write strobes, bank selection and feedback retain
their 4 ns requirement. DAC setup and hold pass at both initial phase 0 and
the normal 56-step startup shift.
External I/O timing coverage remains incomplete: Vivado reports 14 inputs and
41 outputs without delay constraints. The passing checks use the existing board
constraints; external-interface validation still requires hardware measurements.

Production RTL, independent Q8 gain-product vectors, table programming,
host/web and shared-monitor checks pass. The frontend response simulation covers
a 31.25 MHz carrier, arbitrary lock phase, a 10-degree phase step and both mode
handoffs. Other carriers and hardware lock, phase noise, stability and analog
latency have not been characterized. The new instrument has not been installed
on a board. These timing results qualify the original PR 782 revision, not the
combined extractor/programmer design; the combined build is recorded separately.
