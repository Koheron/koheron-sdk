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

Gains use a sign and a base-2 exponent in increments of 1/16 (16 steps per
octave, about 4.43% between steps). The applied gain appears below the exponent.
Coefficient approximation error is at most 0.020373%. Sign 0 disables the gain.
Press Apply to send a gain;
Enter also applies and Escape cancels an exponent draft. Drafts survive polling
and stay with their ADC channel. Gain magnitudes retain the signed 32-bit limits.
The positive exponent limit is 30.9375; negative gains also allow 31.
Integrator checkboxes, DAC routes and the 10 MHz clock reference apply directly.

Startup reads existing settings without changing them. Connection failures
disable controls and expose Retry. Closing the page stops polling and cancels
queued frequency edits. The loop signal-path diagram is packaged with the UI.

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
directly and retains the delays below.

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
| RF DAC mux | 1 | 1 | 1 | 1 |
| **Total clocks / time** | **14 / 56 ns** | **20 / 80 ns** | **33 / 132 ns** | **34 / 136 ns** |

From filtered I/Q through the selector, Fast P is seven clocks (28 ns) and
Fast I is thirteen (52 ns). The accurate I² branch adds five clocks (20 ns) after
the corresponding direct accurate contribution through the first accumulator
and I² gain. In Fast mode, its output also passes through the I + I² preparation
register. I³ has its own gain and accumulator before the precision DAC;
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
plot, precision widget, numeric editors and exports. A separate ordered socket
keeps monitor replies out of the feedback-control command queue.

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
bash examples/alpha250/dpll/tests/run-monitor.sh
```

The host script requires a C++20 compiler and `typescript` and `jsdom`
(listed in `web/package.json`, install there with `npm install`). Set `NODE_PATH`
if they are installed elsewhere. It tests coefficient generation and the
acknowledged programming protocol with address/undefined-behavior sanitizers.
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
gains use four-clock table reductions. All paths accept one sample per clock.
The third table pipeline stage also computes the carry from discarded lower
bits, shortening the final gain sum without changing numerical precision or
latency.
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
preserving PR 782's two-clock RAM setup budget. Acknowledgement follows the write
or commit. Reset preserves committed gains while cancelling pending transfers.
Only these held RAM programming inputs use two-clock timing constraints; the
write strobe, bank commits, lookup addresses and feedback remain at 250 MHz.

The AXI/controller integration simulation runs two controllers against independent
signed-product/state models while programming gains, including reset, rejected
writes and held commands with changing data:

```sh
export DPLL_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh
bash examples/alpha250/dpll/tests/run-table-system.sh
```

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
The monitor stream simulation uses the shared CIC RTL and imports the production
clock converter and FIR configurations. It checks ordering, sustained throughput at R=4/20/8192,
sample-gap reporting under backpressure and recovery after an epoch reset.
Build results and hardware measurements are reported separately in the
[latency notes](tests/gain_latency/README.md#integration-and-hardware-status).

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
