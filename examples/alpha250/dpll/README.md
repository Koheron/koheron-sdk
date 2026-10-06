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

## Continuous phase-noise monitor

The spectrum panel uses the standard PNA implementation: channel selection,
CIC decimation, 1–100 rolling averages, phase/frequency noise, smoothing,
zoom/Fit, frozen reference traces, CSV/PNG export, carrier power and integrated
phase/time jitter with its actual integration band. Precision is selectable
from standard through eight extra fractional bits. The reference is the
selected loop's existing DDS frequency; set it close to the input carrier.

The monitor has a separate measurement path, using the shared
[`pna_cordic.tcl`](../../../fpga/lib/pna_cordic.tcl) and
[`pna_single_stream.tcl`](../../../fpga/lib/pna_single_stream.tcl):

```text
ADC + loop reference DDS
  → 24-bit complex mixer
  → fourth-order 16-sample moving-average prefilter (no interstage truncation)
  → fully pipelined 24-bit CORDIC
  → stochastic phase rounding + independent 64-bit phase history
  → full-precision six-stage fixed CIC /2 (38 bits)
  → clock crossing to 143 MHz
  → six-stage programmable CIC /(R/2), normalized to 40 bits
  → 40-bit compensation FIR /2 → packet quantizer → cyclic SG DMA → DDR
```

The mixer, prefilter, CORDIC and first decimator accept every 250 MS/s input
sample. The programmable CIC, FIR, packet quantizer and DMA run at 143 MHz. Their pipeline latency
is outside the feedback path. The two feedback detectors and controllers keep
their existing arithmetic and pipeline stages. Routed timing still needs to be
checked whenever monitor logic changes placement or routing.

Monitor resets only affect its phase history, filters, FIFO and DMA. Channel,
decimation and precision changes never reset a feedback accumulator, modify a
loop gain, switch a DAC route, or retune a reference. The monitor also measures
with loop integrators disabled. Loop setting changes invalidate old averages
and start a new monitor epoch. Input selection is one ADC/DDS pair at a time.

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

The original `Dma` RPC IDs 0–3 and one-million-int32 raw capture response are
retained. `get_data()` now copies consecutive windows from the running ring.
Check `get_raw_capture_valid()` before using that legacy capture: an interrupted
capture returns zeros and a false validity flag. For calibrated data with
validity and precision in the same reply, use `get_phase_snapshot()`; for the
spectrum use `get_spectrum_snapshot()`. Raw `DmaS2MM.start_transfer` is disabled
on SG hardware to protect the continuous ring. New monitor RPCs are appended.

## Host checks

```sh
make CFG=examples/alpha250/dpll/config.mk web server drivers_json
bash examples/alpha250/dpll/tests/run-host.sh
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
P/PI gains take two clocks and I2/I3 take three. Combining the fast summing node
and accumulator removes another clock: fast correction arrives two clocks
(8 ns at 250 MHz) earlier than the previous controller. The CORDIC is unchanged.
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
