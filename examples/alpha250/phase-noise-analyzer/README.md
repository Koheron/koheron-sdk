# ALPHA250 phase-noise analyzer

## DAC phase-modulated stimulus

The design includes the shared two-channel [DDS phase-modulator IP](../../../fpga/ip/awg_v1_0/)
on DAC0/DAC1, running at the analyzer's existing **200 MS/s**. The two original
DDSs remain unmodulated references for ADC phase extraction. This separation
lets an electrical loopback retain the injected PM in the measured phase.
Changing a local oscillator no longer changes a DAC output.

Open **DAC signal generator** above the plot to use the shared compact widget.
The page follows the ALPHA250 FFT and ALPHA250-4 analyzer workspace: a slim
header, horizontal acquisition/reference controls, a full-width plot, inline
jitter readouts and secondary laser settings. Phase/frequency display and
CSV/PNG export stay beside the plot; controls remain disabled until connected.
It starts collapsed to preserve plot space. The generator reads existing settings
when the page loads and on Refresh, supports keyboard/digit/wheel editing, and uses the same
checked `PhaseModulator` RPC as the standalone example. Outputs start muted after
FPGA reset; explicitly prepare and enable the desired signal. Carrier amplitude
is full digital scale; the amplitude field sets phase deviation in degrees.

For a loopback, connect a DAC to its corresponding ADC and match its carrier to
that ADC channel's **Local Oscillator**. PM affects the stimulus only. Choose
modulation frequencies within the acquisition filter's passband. **Save Analyzer
Config** saves the analyzer settings; generator settings are not persisted by it.

The existing Python phase-modulator client can share the analyzer's connection.
From the SDK root, after installing this instrument on a board:

```python
import sys
sys.path.insert(0, "examples/alpha250/phase-noise-analyzer")
sys.path.insert(0, "examples/alpha250/phase-modulator")
from koheron import connect
from phase_noise_analyzer import PhaseNoiseAnalyzer
from phase_modulator import PhaseModulator

client = connect("BOARD_IP", name="phase-noise-analyzer")
analyzer = PhaseNoiseAnalyzer(client)
generator = PhaseModulator(client)
analyzer.set_local_oscillator(0, 10_000_000)
generator.configure(channel=0, carrier_hz=10_000_000,
                    modulation_hz=10_000, deviation=1,
                    output_enabled=True, pm_enabled=True)
print(generator.settings(0))
generator.mute(0)
```

The DAC subsystem occupies `0x44000000`–`0x44001fff`; existing analyzer register
addresses and acquisition processing remain unchanged. The reusable ALPHA250
RPC driver lives in `boards/alpha250/drivers/phase-modulator.hpp` and selects the
host instrument's sample clock, including 200 MS/s here and 250 MS/s in the
standalone example.

## Phase conversion

The server converts filtered DMA counts to radians before returning `get_phase()`
or computing phase-noise PSD and jitter. Clients must not apply another phase
calibration to those outputs.

The CORDIC/unwrapper scale is `pi / 8192` radians per unfiltered count. For the
current six-stage CIC with differential delay 1 and 32-bit input/output, the
low-frequency filter correction at CIC rate `R` is:

```text
C(R) = 4 * 2^ceil(log2(R^6)) / R^6
phase_radians = filtered_DMA_counts * C(R) * pi / 8192
```

The factor 4 compensates the FIR's fixed-point scaling: 32 fractional coefficient
bits, a 66-bit accumulator, and a 32-bit output give a DC gain of approximately
1/4. The second factor compensates the CIC's power-of-two truncation of its
full-precision gain `R^6`. See [AMD PG140](https://docs.amd.com/r/en-US/pg140-cic-compiler/Output-Width-and-Gain)
and [PG149](https://docs.amd.com/r/en-US/pg149-fir-compiler/Output-Width-and-Bit-Growth).

At rate 20, the correction is 4.194304, replacing the former fixed 4.196 value
(about -0.0404% in phase amplitude and -0.00351 dB in phase PSD). At power-of-two
rates the correction is 4. Phase PSD scales with the square of this correction;
phase and time jitter scale linearly. Rate changes and phase processing share
the acquisition mutex, and the existing two-transfer settling discard is retained.

This correction assumes the current CIC/FIR configuration and concerns gain near
DC. It does not compensate passband frequency response or the optical delay-line
transfer function. The fixed-point widths were checked with isolated IP generated
in Vivado 2026.1; verification with a known electrical phase modulation on hardware
remains necessary. See [issue #711](https://github.com/Koheron/koheron-sdk/issues/711).

## Validation

Run the host regression from the repository root (Python 3 and a C++20 compiler):

```sh
CXX=g++-13 python3 examples/alpha250/phase-noise-analyzer/tests/test_phase_calibration.py
CXX=g++-13 python3 examples/alpha250/phase-noise-analyzer/tests/test_phase_modulator.py
NODE_PATH=web/node_modules node --test examples/alpha250/phase-noise-analyzer/tests/test_signal_generator.cjs
make CFG=examples/alpha250/phase-noise-analyzer/config.mk server N_CPUS=2
make CFG=examples/alpha250/phase-noise-analyzer/config.mk web
make CFG=examples/alpha250/phase-noise-analyzer/config.mk fpga N_CPUS=4
make CFG=examples/alpha250/phase-noise-analyzer/config.mk timing
```

The regression checks every supported rate against exact integer CIC gain,
including the maximum rate, power-of-two rates, and truncation-bit boundaries.
The simulated-MMIO regression also checks sample-clock selection, read-only
generator discovery, mute/resume and independence of DAC/reference settings.
Host web checks cover the collapsed panel, checked DAC editing, retry and teardown.
FPGA builds enforce routed timing, including the packaged CDC bus-skew constraints.
After `make xpr`, check the actual block-design connections and address window:

```sh
source /tools/Xilinx/2025.1/Vivado/settings64.sh
vivado -mode batch -source examples/alpha250/phase-noise-analyzer/tests/check_fpga.tcl \
  -tclargs tmp/examples/alpha250/phase-noise-analyzer/fpga/phase-noise-analyzer.xpr
```

On a board, apply a known low-frequency electrical phase modulation, compare
phase amplitude at rates 16, 20, and 32, and switch rates during acquisition.
Check that settled phase, PSD, and jitter agree with the expected modulation.
Sweep modulation frequency separately to assess the measurement passband.
