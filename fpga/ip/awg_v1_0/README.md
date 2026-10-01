# AXI DDS phase modulator

A configurable Vivado IP controller for a Xilinx DDS Compiler carrier, with
internal phase modulation generated on every sample clock. The default carrier
and modulation phase accumulators are **48 bits**. The first target is ALPHA250;
the controller and shared driver have no board-specific DAC formatting.

This implements internal PM and internally timed BPSK on a sine carrier.
External modulation is left to the Xilinx streaming interface, as requested.
AM, FM, sweeps, bursts, uploaded samples and non-sinusoidal carriers are future
work, not features of this implementation. The Keysight 33600A is a functional
reference; this is not a claim of instrument equivalence or analog accuracy.

## Internal modulation

| Code | Source | Behavior |
| --- | --- | --- |
| 0 | Sine | Xilinx sine lookup table, driven by a 48-bit modulation oscillator |
| 1 | Square | Exact positive and negative phase deviation; 50 percent duty |
| 2 | Pulse | Exact positive and negative deviation; programmable 0 to 100 percent duty |
| 3 | Triangle | Starts at negative deviation, reaches positive deviation halfway through the cycle |
| 4 | Up ramp | Negative deviation toward positive deviation |
| 5 | Down ramp | Positive deviation toward negative deviation |
| 6 | Uniform noise | Seeded xorshift32, updated once per modulation-oscillator wrap |
| 7 | Approximate Gaussian noise | Sum of twelve 8-bit uniform samples; updated once per wrap |
| 8 | PRBS | Seeded PN7, PN15, PN23 or PN31, one new bit per wrap |
| 9 | BPSK | Alternates between zero and the configured phase shift, one symbol per wrap |

For ordinary PM, `POFF[n] = carrier_phase + deviation * m[n]`, modulo one turn.
Carrier frequency stays constant. Deviation is a positive magnitude from zero
to one full turn (360 degrees). BPSK instead selects zero or that phase shift.
Modulation frequency means waveform cycles per second for periodic sources,
update rate for noise/PRBS, and symbol rate for BPSK.

Uniform samples are normalized to `[-1, 1)`. Gaussian noise is an explicitly
bounded approximation with range `[-3060/4096, 3060/4096]` and nominal standard
deviation about `1/8`; deviation denotes the scale applied to those normalized
samples, not their RMS value. Neither noise source implements a programmable
analog bandwidth filter. Samples are held between updates. Nonzero seeds make
restart deterministic. PRBS polynomials are x^7+x^6+1, x^15+x^14+1,
x^23+x^18+1 and x^31+x^28+1. Gaussian noise and PRBS can be omitted independently.

## Precision and throughput

Native phase settings have `2^-48` turn resolution by default, about
`1.279e-12` degrees. Two 32-bit AXI writes preserve each word through an atomic
commit. The driver accepts native integer words; the Python client also accepts
Decimal or string phase values without rounding to tenths of a degree.

A full-turn deviation needs **49 bits**, including the value `2^48`. The scaler
retains that extra bit before multiplying and rounding, then wraps the final
phase offset. Square, pulse and PRBS use exact endpoints; BPSK also bypasses the
multiplier. The general scaler uses three 17-bit deviation limbs and three
DSP48 multipliers at the default widths, with a seven-clock pipeline.

Phase-setting precision is separate from waveform precision. By default,
modulation amplitude samples have 24 bits and the internal sine LUT uses 14
address bits. Changing the 48-bit modulation phase offset by less than a LUT
address step may leave a sine sample unchanged. The carrier uses Xilinx phase
dithering; its 16-bit output cannot represent 48-bit analog phase accuracy.

The interfaces are continuous AXI4-Stream interfaces without TREADY. The helper
configures both vendor IPs accordingly; neither stream may be stalled. A setting
tag travels through the modulation LUT and scaler alongside its sample, so a
commit cannot mix old frequency, phase, deviation or output-enable settings.
A five-clock source pipeline accommodates the Gaussian adder tree; phase,
other sources and settings receive the same delay.
The carrier DDS carries output enable in TUSER, preserving mute timing.

## Vivado customization

The packaged controller is `koheron:user:awg:1.0`. Its GUI exposes precision
and internal-source selection. Disabling a source at build time removes its
logic; disabling PM through AXI keeps the compiled circuitry present.

| Parameter | Default | Range or purpose |
| --- | --- | --- |
| PHASE_WIDTH | 48 | 32 to 48; carrier and modulation phase precision |
| OUTPUT_WIDTH | 16 | 12 to 24; signed carrier/DAC sample width |
| MOD_WIDTH | 24 | 16 to 24; modulation sample precision |
| LUT_BITS | 14 | 8 to 18; internal modulation sine table address width |
| PRBS_WIDTH | 31 | 7, 15, 23 or 31 |
| ENABLE_SINE through ENABLE_BPSK | 1 each | Include or remove each source independently |

The controller is packaged separately from the vendor DDS instances. Use the
integration helper to instantiate the subsystem consistently; its two DDS IPs
remain standard Xilinx IPs. This keeps the IP portable across Vivado versions
without committing generated vendor netlists or duplicating the DDS internals.
There is one controller and one carrier DDS per simultaneous output channel.
The modulation LUT is omitted when ENABLE_SINE is zero.

## Example integration

In the instrument's `config.mk`:

```make
CORES += $(SDK_PATH)/fpga/ip/awg_v1_0
TCL_FILES += $(SDK_PATH)/fpga/ip/awg_v1_0/integration.tcl
```

Declare a 4 KiB, read/write region named `awg0` in `memory.yml`. After the board
starting point, instantiate and connect one channel in `block_design.tcl`:

```tcl
source $sdk_path/fpga/ip/awg_v1_0/integration.tcl
set options [dict create ENABLE_GAUSSIAN 0 ENABLE_UNIFORM 0]
set output [dds_pm::add awg0 awg0 adc_dac/adc_clk [get_parameter adc_clk] $options]
connect_pins $output adc_dac/dac0
```

The helper connects AXI clock/reset and assigns the declared region. It returns
signed DAC samples. Repeat with distinct instance and region names for another
channel. Channels run independently; sequential AXI commits do not provide a
simultaneous two-channel restart.

The shared C++ driver is
[`server/drivers/dds/phase-modulator.hpp`](../../../server/drivers/dds/phase-modulator.hpp).
It validates capabilities and native ranges, serializes complete configurations,
orders MMIO writes before commit and reports a timeout if the sample clock stops.
The [ALPHA250 example](../../../examples/alpha250/phase-modulator/) supplies the
RPC wrapper, memory map, board clock selection and Python client.

## Register map

All accesses are 32-bit AXI4-Lite with byte strobes. Misaligned, unmapped and
read-only writes return SLVERR. Shadow registers read back their staged values;
unused high bits are masked to zero. They do not read back the active state.

| Offset | Register | Meaning |
| --- | --- | --- |
| 0x00 | ID | Read-only `0x504d0001` |
| 0x04 | Capabilities | Read-only bit per source code, bits 0 through 9 |
| 0x08 | Precision | Bytes: phase width, modulation width, LUT bits, PRBS order |
| 0x0c | Status | Bit 0: commit pending |
| 0x10 | Command | Bit 0 commit, bit 1 carrier restart, bit 2 modulation restart |
| 0x18 | Output width | Read-only carrier sample width |
| 0x20 / 0x24 | Carrier increment | Low/high native phase-frequency word |
| 0x28 / 0x2c | Carrier phase | Low/high fixed phase offset |
| 0x30 / 0x34 | Modulation increment | Low/high modulation rate word |
| 0x38 / 0x3c | Modulation phase | Low/high modulation oscillator phase offset |
| 0x40 / 0x44 | Deviation | Low/high magnitude; full turn is permitted |
| 0x48 / 0x4c | Pulse duty | Low/high fraction of a turn; full turn means 100 percent |
| 0x50 | Seed | Noise seed or low PRBS-order bits; defaults to 1 |
| 0x54 | Control | Bit 0 output enable, bit 1 PM enable, bits 11:8 source code |

Frequency is `increment * sample_rate / 2^PHASE_WIDTH`. Fixed phase offsets
wrap modulo one turn. Pulse duty defaults to 50 percent; outputs and PM default
to disabled. A commit with invalid deviation, duty, source or seed returns
SLVERR. A second commit while busy also returns SLVERR, without replacing the
pending configuration. Editing shadow registers while busy is permitted.
Command restart bits require the commit bit.

Commit acknowledgement means the coherent settings have reached the sample
clock domain, not that their first sample has reached the DAC. Processing and
DDS pipelines add latency. Carrier restart begins at the requested phase by
sending zero PINC on the RESYNC sample; following samples use normal PINC.
Modulation restart resets its oscillator and reseeds noise/PRBS. Mute does not
stop either oscillator. Changing settings without restart preserves accumulated
phase, but changes of offset, depth or source can create a phase discontinuity.

The controller derives a synchronized sample-domain reset from AXI reset.
Asserting AXI reset discards shadow, active and pending settings and mutes output.
The packaged CDC constraints bound stable mailbox data and constrain request
and acknowledgement synchronizers. They must remain included in the design.
Their 8 ns mailbox delay and 4 ns skew budgets support sample clocks up to
250 MHz; the helper rejects higher rates. They are conservative at slower
clocks, including the intended Red Pitaya target.

## Offline validation

```sh
.venv/bin/python -m pytest fpga/ip/awg_v1_0/tests/test_client.py -q
g++ -std=c++20 -Wall -Wextra -Werror -fno-exceptions -pthread -I. \
    fpga/ip/awg_v1_0/tests/driver_test.cpp -o /tmp/dds-pm-driver-test
/tmp/dds-pm-driver-test
source /tools/Xilinx/2025.1/Vivado/settings64.sh
vivado -mode batch -nolog -nojournal -source fpga/ip/awg_v1_0/tests/run_sim.tcl
vivado -mode batch -nolog -nojournal -source fpga/ip/awg_v1_0/tests/run_control.tcl
vivado -mode batch -nolog -nojournal -source fpga/ip/awg_v1_0/tests/synth.tcl
```

The integration simulation uses the actual Xilinx carrier DDS and modulation
LUT. It checks every source, stream tags, PM scaling, one-LSB and full-turn
settings, pulse endpoints, AXI channel skew/backpressure, reset and mute.
Reduced-controller tests cover coherent commits while the sample clock is
stopped and byte-aligned/non-aligned phase widths. Resource reports generated
by `synth.tcl` cover the controller only, excluding vendor DDSs and board logic.

Vivado 2025.1 out-of-context synthesis on XC7Z020-2, default precision:

| Controller profile | LUTs | Flip-flops | DSP48 | BRAM |
| --- | ---: | ---: | ---: | ---: |
| All internal sources | 2,193 | 2,711 | 3 | 0 |
| Without Gaussian, uniform noise and PRBS | 1,283 | 2,031 | 3 | 0 |
| All PM sources disabled | 510 | 858 | 0 | 0 |

These counts exclude the carrier DDS and optional modulation sine LUT; they
are synthesis estimates, with placement and fanout replication affecting the
final counts. To check the reduced integration against the packaged catalog:

```sh
vivado -mode batch -nolog -nojournal -source fpga/ip/awg_v1_0/tests/package_smoke.tcl \
    -tclargs tmp/examples/alpha250/phase-modulator/cores
```

The default two-channel ALPHA250 design was placed and routed at 250 MHz with
Vivado 2025.1. The SDK's strict timing check passes: setup margin 0.099851 ns,
hold margin 0.016769 ns, and eight analyzed bus-skew constraints. The example
runs setup optimization followed by hold repair and preserved rerouting.
Including the vendor DDSs, each channel uses three DSP48s and five BRAM36s
(two for the carrier and three for the modulation sine LUT).

The default board constraints leave 14 inputs and 41 outputs without I/O delay
constraints; these timing results cover the constrained FPGA paths. Physical
board measurements, interface margins and analog PM fidelity are not yet
validated.
