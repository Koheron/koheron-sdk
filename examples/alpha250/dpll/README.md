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

## Host checks

```sh
make CFG=examples/alpha250/dpll/config.mk web server drivers_json
bash examples/alpha250/dpll/tests/run-host.sh
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
loop stability or FPGA timing. FPGA simulation checks remain in `tests/run-fpga.sh`.

## Gain implementation and APIs

Both controllers use double-buffered lookup tables prepared when a gain changes.
P/PI gains take two clocks and I2/I3 take three. Combining the fast summing node
and accumulator removes another clock: fast correction arrives two clocks
(8 ns at 250 MHz) earlier than the previous controller.
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

## Phase extraction

Both loops retain 24-bit I/Q and produce **24-bit phase** with pi = 2^21.
The extractor normalizes into 27-bit coordinates, performs eight CORDIC rotations
with a 32-bit internal angle accumulator, then applies a three-clock residual
correction using an interpolated reciprocal. It accepts one sample per clock
and takes **14 clocks (56 ns)** at 250 MHz,
compared with 28 clocks for the PNA's 24-bit vendor CORDIC configuration.
The independent atan2 simulation checks 192,533 samples: peak error is
**1.115 µrad**, RMS error is **0.405 µrad**. These are arithmetic errors, not
hardware phase-noise measurements.

Unwrapping retains 40-bit phase and 25-bit frequency. Controllers carry the eight
extra fractional bits through every product and accumulator, removing them at
the DAC output. Existing gain settings retain their physical scale. Compatibility
outputs keep the existing monitor and direct phase-DAC units; controllers use the
full-precision feedback pins. Monitoring sources are unchanged.
The vendor core and 16-bit phase configuration remain regression references.
Experimental compact preparation is disabled; it has not been qualified at this
precision.

The full phase detector takes 23 clocks (92 ns). The direct phase-feedback
converter/pipeline subtotal is 164 ns; add 6 ns boxcar group delay and the
board/interface/analog delays. These are pipeline counts, not measured connector
latency or closed-loop bandwidth. Only standalone extractor timing was checked
for the current design; full-instrument timing and hardware testing are deferred.
See the [phase extraction checks and results](tests/phase_extraction/README.md).

```sh
export DPLL_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh
DPLL_PHASE_ROUTE=1 bash examples/alpha250/dpll/tests/phase_extraction/run.sh
```

## Full FPGA build

```sh
make -j4 CFG=examples/alpha250/dpll/config.mk all
make CFG=examples/alpha250/dpll/config.mk timing
vivado -mode batch -nolog -nojournal -notrace \
  -source examples/alpha250/dpll/tests/check_table_design.tcl \
  -tclargs tmp/examples/alpha250/dpll/fpga/dpll.xpr \
  tmp/tests/alpha250-dpll/full-design
```

The normal build enforces routed setup, hold, pulse-width and bus-skew checks
before writing the bitstream. The additional design check verifies the full
instrument top, 250 MHz clocks, both phase extractors, both selected controllers
and all eight table gain paths.
Build results and hardware measurements are reported separately in the
[latency notes](tests/gain_latency/README.md#integration-and-hardware-status).
