# AXI DDS phase modulator

A complete Vivado IP with one or two independent Xilinx DDS Compiler carriers
and internal phase modulation generated on every sample clock. A single
AXI4-Lite interface controls the whole subsystem. The default carrier
and modulation phase accumulators are **48 bits**. The first target is ALPHA250;
the controller and shared driver have no board-specific DAC formatting.

This implements internal PM and internally timed BPSK on a sine carrier.
The DDS streaming interfaces are internal to the packaged IP; external
modulation is not exposed by this version.
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

The internal connections are continuous AXI4-Stream interfaces without TREADY.
The packager configures both vendor IPs accordingly; neither stream may be
stalled. A setting tag travels through the modulation LUT and scaler alongside its sample, so a
commit cannot mix old frequency, phase, deviation or output-enable settings.
A five-clock source pipeline accommodates the Gaussian adder tree; phase,
other sources and settings receive the same delay.
The carrier DDS carries output enable in TUSER, preserving mute timing.

## Vivado customization

The packaged subsystem is `koheron:user:awg:1.0`. Its GUI exposes channel count
and internal-source selection, and displays the packaged precision. Disabling a
source at build time removes its logic; disabling PM through AXI keeps the compiled circuitry present.

| Parameter | Default | Range or purpose |
| --- | --- | --- |
| CHANNELS | 1 | 1 or 2 independent output channels |
| PHASE_WIDTH | 48 | 32 to 48; carrier and modulation phase precision |
| OUTPUT_WIDTH | 16 | 12 to 24; signed carrier/DAC sample width |
| MOD_WIDTH | 24 | 16 to 24; modulation sample precision |
| LUT_BITS | 14 | 8 to 18; internal modulation sine table address width |
| PRBS_WIDTH | 31 | 7, 15, 23 or 31 |
| ENABLE_SINE through ENABLE_BPSK | 1 each | Include or remove each source independently |

Each channel contains the AXI controller, carrier DDS and optional modulation
sine LUT. All stream connections are internal. The top-level interfaces are
`S_AXI`, `s_axi_aclk`, `s_axi_aresetn`, `sample_clk`, `dac0_data` and, when
`CHANNELS=2`, `dac1_data`. Both channels share the sample clock and build-time
source options, but retain independent settings, accumulators, commits and mute.
The unused second channel and disabled modulation sine LUT are removed during
synthesis.

`package_ip.tcl` generates the two vendor DDS configurations and the SDK
packager imports their `.xci` files into the parent IP. No generated vendor
netlists are committed. Vivado regenerates output products for the package.

The four precision parameters are set in `package_settings.tcl` **when generating
the package**, then fixed on catalog instances so the wrapper and its embedded
DDSs cannot disagree. Edit that dictionary and rerun `make cores` for a different
precision profile. Channel count, PRBS order and source switches remain
configurable on individual instances. Vendor DDSs are configured for up to
250 MHz. Rebuild the package when changing Vivado releases to regenerate the
child configurations with that release.

## Example integration

In the instrument's `config.mk`:

```make
CORES += $(SDK_PATH)/fpga/ip/awg_v1_0
TCL_FILES += $(SDK_PATH)/fpga/ip/awg_v1_0/integration.tcl
```

Declare an 8 KiB, read/write region named `awg` in `memory.yml`. After the board
starting point, instantiate and connect the subsystem in `block_design.tcl`:

```tcl
source $sdk_path/fpga/ip/awg_v1_0/integration.tcl
set options [dict create CHANNELS 2 ENABLE_GAUSSIAN 0 ENABLE_UNIFORM 0]
set outputs [dds_pm::add awg awg adc_dac/adc_clk [get_parameter adc_clk] $options]
connect_pins [lindex $outputs 0] adc_dac/dac0
connect_pins [lindex $outputs 1] adc_dac/dac1
```

The helper connects AXI clock/reset and assigns the declared region. It returns
a list of one or two signed DAC sample pins. Use `CHANNELS 1` for a single
output. The address window remains 8 KiB in both configurations; accesses to
the absent second bank return SLVERR. Channels run independently; sequential
AXI commits do not provide a simultaneous two-channel restart.

The shared C++ driver is
[`server/drivers/dds/phase-modulator.hpp`](../../../server/drivers/dds/phase-modulator.hpp).
One `dds_pm::Controller<Memory>` owns the entire 8 KiB region. It discovers the
channel count before reading the second bank and validates identifier, precision,
output width, PRBS order and capabilities for every present channel. Invalid or
inconsistent metadata makes the entire controller unavailable; an absent bank
is never accessed. The [ALPHA250 example](../../../examples/alpha250/phase-modulator/)
supplies the RPC wrapper, memory map, board clock selection and Python client.

```cpp
dds_pm::Controller controller(memory, 250'000'000.0L); // Sampling rate in Hz
if (!controller.valid()) {
    report_error(controller.initialization_result().message());
    return;
}
dds_pm::SignalSettings signal; // 10 MHz tone, output enabled, PM off
signal.pm_enabled = true;     // Default: 1 kHz sine PM, 30 degree deviation
const auto result = controller.configure_signal(0, signal);
if (!result) report_error(result.message());
```

`SignalSettings` expresses frequencies in Hz, phase offsets and deviation in
degrees, and pulse duty as a fraction from 0 to 1. Frequencies must be
nonnegative and below Nyquist. Phase offsets wrap around one turn; deviation
allows 0 through 360 degrees. Invalid and nonfinite settings return an error
before any setting writes. Supply the sampling rate in the constructor, or
explicitly to `configure_signal` and the frequency setters. The controller
does not program the board clock.

`set_carrier_frequency`, `set_modulation_frequency`, `set_phase`,
`set_deviation`, `set_pm_enabled` and `set_output_enabled` change one setting
while preserving the others and the running oscillator state. `mute(channel)`
disables the output; `set_output_enabled(channel, true)` resumes it with the
same settings. `restart(channel)` explicitly restarts both oscillators.
All these operations return the same checked `Result` as full configuration.
Engineering-unit full configuration restarts both oscillators by default;
the overload with an explicit sampling rate accepts separate restart flags.

For exact native words, use `default_settings(channel)` and
`configure(channel, settings, restart_carrier, restart_modulation)`.
These defaults are muted, with PM off and 50 percent pulse duty; native
configuration preserves oscillator state unless restart flags are set.
Native partial setters are also available. `get_settings(channel, settings)`
waits for any pending commit and reads the hardware shadow words under the
channel lock. With one controller owning the region, this gives the accepted
configuration, including after a timeout recovers. It does not measure the
output or read the running phase accumulator.

`channel_count()`, `channel_info(channel)`, `phase_width(channel)`,
`capabilities(channel)` and `full_turn(channel)` expose the discovered hardware.
`configure(channel, settings, restart_carrier, restart_modulation)` returns a
`Result` with an `Error` code and a stable message belonging to that call.
Use `static_cast<bool>(result)` when an explicit Boolean return is needed.
`configure(settings, ...)` addresses channel 0. `error()` reports the most recent
operation for compatibility; concurrent callers should use their own `Result`.
Native integer words remain available independently of engineering-unit
conversion. The Python client uses Decimal conversion to preserve fine phase
and frequency settings through RPC.

Each bank has its own mutex, preventing shadow words from different calls on
the same channel from mixing while allowing independent channels to configure
concurrently. Use one controller object per mapped IP region. Validation fails
before any setting writes. MMIO fences order the shadow writes before commit.
The optional constructor timeout defaults to 100 ms and covers both hardware
waits; polling sleeps between busy reads rather than consuming a CPU core.
`pending_commit_timeout` means no new settings were written because an earlier
commit remained pending. `commit_timeout` means this call issued a commit but
its acknowledgement did not arrive: the settings can still apply when the
sample clock resumes. The driver never automatically retries a commit.

## Register map

The single AXI4-Lite interface uses a 13-bit address and an 8 KiB window.
Channel 0 occupies offsets `0x0000–0x0fff`; channel 1 occupies
`0x1000–0x1fff`. The table below uses offsets relative to each channel bank.
The AXI router buffers write address and data independently, preserves responses
under backpressure, and permits an independent read while a write is pending.

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
| 0x1c | Channel count | Read-only 1 or 2, identical in both present banks |
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
make cores CFG=examples/alpha250/phase-modulator/config.mk VIVADO_VERSION=2025.1
vivado -mode batch -nolog -nojournal -source fpga/ip/awg_v1_0/tests/run_subsystem.tcl \
    -tclargs tmp/examples/alpha250/phase-modulator/cores
```

The controller integration simulation uses the actual Xilinx carrier DDS and
modulation LUT. It checks every source, stream tags, PM scaling, one-LSB and
full-turn settings, pulse endpoints, AXI channel skew/backpressure, reset and mute.
Reduced-controller tests cover coherent commits while the sample clock is
stopped and byte-aligned/non-aligned phase widths. Resource reports generated
by `synth.tcl` cover the controller only, excluding vendor DDSs and board logic.

`run_subsystem.tcl` additionally instantiates the actual packaged catalog IP and
simulates/synthesizes one-channel, two-channel and no-sine profiles. It checks
address/data skew, byte strobes, absent-bank errors, independent outputs and
commits, concurrent reads with backpressured writes, reset and hardware pruning.
An alternate packaged profile with 33-bit phase, 14-bit output, 16-bit modulation
and 8-bit LUT addressing also passes this simulation and synthesis.

Vivado 2025.1 out-of-context synthesis on XC7Z020-2, default precision,
including the embedded DDSs and shared AXI router:

| Subsystem profile | LUTs | Flip-flops | DSP48 | BRAM36 |
| --- | ---: | ---: | ---: | ---: |
| One channel, all sources | 2,707 | 3,626 | 3 | 5 |
| Two channels, all sources | 5,377 | 7,196 | 6 | 10 |
| Two channels, sine source omitted | 4,613 | 5,942 | 6 | 4 |

These are synthesis estimates; placement and fanout replication affect final
counts. The tests also verify that synthesis contains exactly `CHANNELS`
controllers and no unresolved vendor IP black boxes. To validate block-design
integration against the packaged catalog:

```sh
vivado -mode batch -nolog -nojournal -source fpga/ip/awg_v1_0/tests/package_smoke.tcl \
    -tclargs tmp/examples/alpha250/phase-modulator/cores
```

The integrated two-channel ALPHA250 design was placed and routed at 250 MHz
with Vivado 2025.1. The SDK's strict timing check passes: setup margin
**0.000620 ns**, hold margin **0.044454 ns**, and eight analyzed bus-skew
constraints. The setup margin is very narrow; changes to the package or its
precision profile require rerouting and another timing check.

The default board constraints leave 14 inputs and 41 outputs without I/O delay
constraints; timing results cover the constrained FPGA paths.

## Live ALPHA250 validation

The complete instrument was installed and started on an ALPHA250. The client
discovered both 48-bit channels and all ten modulation sources, configured
DAC0 for a 10 MHz carrier with 10 kHz sine PM and 1 degree deviation, and read
back the acknowledged native settings. Output and PM were enabled on DAC0;
DAC1 remained muted. This checks the packaged bitstream, server and Python
client together on hardware. Interface margins and analog PM amplitude,
spectrum and fidelity have not yet been measured.
