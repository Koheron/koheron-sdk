# ALPHA250 phase-noise analyzer

## FPGA phase-extraction filter

Version 1.2.0 carries over the ALPHA250-4 analyzer's mixer prefilter. Each
ADC's I/Q mixer outputs pass through four cascaded 16-sample moving sums before
the CORDIC, replacing the four-sample boxcar. The equivalent 61-tap FIR has
unity DC gain. Intermediate sums retain full precision; the final 16-bit
output uses stochastic rounding. Each ADC channel selects a distinct nonzero
seed for the shared 64-bit XOR LFSR; mixer and I/Q filter rounding use separate
bits of that channel's sequence.

At the existing 200 MS/s sample clock and a 10 MHz carrier, the filter
attenuates the 20 MHz mixing image by 57.27 dB, versus 2.28 dB for the old
boxcar. Its passband loss is 0.091 dB at 500 kHz, 0.365 dB at 1 MHz and
1.470 dB at 2 MHz. Software does not invert this response. The filter is
intended for 10 MHz carriers and sub-MHz offsets; lower carriers and wider
offsets need the filter response taken into account. These filter calculations
do not establish an absolute instrument noise floor.

The two ADC phases still feed the existing channel selector and single
CIC/FIR/DMA path. The ALPHA250-4's paired-CIC controller serves its two
simultaneous phase-difference streams and is not needed for this topology.
Phase conversion retains the existing CIC/FIR correction below, since the
new mixer prefilter has unity DC gain. The independent DAC stimulus and
analyzer register map are retained.

Run the FPGA regressions and build from the SDK root:

```sh
PNA_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh \
    bash examples/alpha250/phase-noise-analyzer/tests/run-fpga.sh
make CFG=examples/alpha250/phase-noise-analyzer/config.mk fpga N_CPUS=4
```

The simulation and ideal mixer model reuse the ALPHA250-4 regressions for the
same shared RTL. See [tests/README.md](tests/README.md) for block-design checks
and a board validation procedure.

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
addresses remain unchanged. The reusable ALPHA250
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
phase_radians = (filtered_DMA_counts - first_count) * C(R) * pi / 8192
```

The factor 4 compensates the FIR's fixed-point scaling: 32 fractional coefficient
bits, a 66-bit accumulator, and a 32-bit output give a DC gain of approximately
1/4. The second factor compensates the CIC's power-of-two truncation of its
full-precision gain `R^6`. See [AMD PG140](https://docs.amd.com/r/en-US/pg140-cic-compiler/Output-Width-and-Gain)
and [PG149](https://docs.amd.com/r/en-US/pg149-fir-compiler/Output-Width-and-Bit-Growth).

At rate 20, the correction is 4.194304, replacing the former fixed 4.196 value
(about -0.0404% in phase amplitude and -0.00351 dB in phase PSD). At power-of-two
rates the correction is 4. Phase PSD scales with the square of this correction;
phase and time jitter scale linearly. The difference from the first DMA count
uses an unsigned integer magnitude before conversion to float, preserving the
full signed-input range without overflow. `get_phase()` therefore starts at
zero and retains the phase drift and modulation, while avoiding
loss of small increments when the unwrap accumulator has a large offset.

Before the existing 32768-point Hann Welch calculation, the server removes a
least-squares constant and linear trend from the 65536-sample phase block.
The fit accumulates exact 64-bit integer sums from the raw DMA counts. Each
FFT worker removes the fitted slope and its segment's mean in double precision
while preparing the Hann-windowed float-radian samples. Segment means are
also accumulated exactly in integer counts. This avoids a separate detrended
array and overlaps sample preparation with the other worker's FFT.
This preserves small phase increments on a large carrier-frequency ramp
and suppresses leakage from carrier/reference frequency mismatch without
modifying the phase snapshot. Detrending changes the response at the lowest
offsets; the plot continues to start at FFT bin 2. It does not correct the
FPGA filter's passband response.

The Welch estimator uses a cached PFFFT plan and aligned buffers with ARM NEON
on the Cortex-A9. Two workers process alternate 50%-overlapped segments. The
Hann window and one-sided density normalization, including DC and Nyquist,
are unchanged. PFFFT is vendored with its license in the instrument archive;
no FFT runtime package is required on the board.

Settings, spectral processing and publication share a lock. DMA waits use a
separate lock so snapshot getters remain available during acquisition. Rate
changes wait for the current DMA transfer. Channel and rate changes discard
two transfers, and LO changes discard four. Failed or timed-out transfers
clear the published data and averages and discard two successful transfers
before publishing again. While results are invalid, phase and PSD are zero
and jitter/integration bounds are NaN. Shutdown still waits for an in-flight
DMA transfer to finish or time out.

The LO driver rounds to the nearest 48-bit DDS tuning word using the ADC clock,
reports the implemented frequency, and saves/loads frequencies as doubles.
Use the analyzer's `set_local_oscillator()` command for changes during
acquisition; the Python `set_dds_freq()` compatibility alias now calls it.
Changing channel, LO, CIC rate, RF/laser mode or interferometer delay clears
averages. Averaging retains the most recent spectrum even with a window of
one, so later growth cannot restore an old spectrum.

`get_parameters()` reports all 16385 PSD bins, including DC and Nyquist.
The web axis uses their actual bin centers. The Python client reads 65536
phase samples, uses the server's sample rate, detrends before its Hann FFT,
and applies one-sided PSD scaling without doubling DC or Nyquist. Its
`get_phase_noise()` returns the current server spectrum in rad²/Hz; the
existing `phase_noise()` method computes a separate client spectrum with a
65536-point FFT.

The plot and decade readouts average linear density over 0.1-decade frequency
windows before converting to dB. The **Smoothed trace** checkbox controls the
overlay; raw PSD, server averaging and jitter are unchanged by display
smoothing. CSV exports include raw and smoothed display values plus the
original server PSD in rad²/Hz, including DC and Nyquist. Plot updates run at
up to 10 Hz, measurement readouts at 4 Hz and settings polling at 2 Hz.

This correction assumes the current CIC/FIR configuration and concerns gain near
DC. It does not compensate passband frequency response or the optical delay-line
transfer function. The fixed-point widths were checked with isolated IP generated
in Vivado 2026.1; verification with a known electrical phase modulation on hardware
remains necessary. See [issue #711](https://github.com/Koheron/koheron-sdk/issues/711).

## Slow local-oscillator tracking

**Slow tracking** is off by default for existing configurations. Enabling it
tracks only the selected ADC's LO; the other LO retains its last correction.
Each channel has a separate correction and lock history. Editing a nominal LO
resets that channel's correction. Turning tracking off restores both nominal
frequencies and discards four transfers. Saving analyzer configuration stores
the nominal frequencies and tracking settings; acquired corrections are not
persisted. A saved enabled setting resumes tracking after instrument startup.

The frequency-error estimate fits the whole 65536-sample phase snapshot before
detrending. In this design, the DDS outputs cosine in the low half and sine in
the high half, as specified in [AMD PG141](https://docs.amd.com/r/en-US/pg141-dds-compiler/Output-DATA-Channel-TDATA-Structure).
Multiplying the real ADC signal by that complex LO gives a measured phase slope
of LO frequency minus input frequency. The loop therefore lowers the LO for a
positive slope and raises it for a negative slope.

Default limits are 0.1 Hz requested bandwidth, 0.05 Hz per update and 100 Hz
total correction from the nominal LO. The bandwidth ceiling is the smaller of
the requested value, 0.1 Hz and one hundredth of the first displayed offset
(`2 * fs / 32768`). The update gain is capped at 0.2 and includes capture and
processing time. Per-step and total bounds apply within DDS tuning-word
rounding. A zero bandwidth, step limit, total limit or nominal LO stops updates.
Lock requires the filtered frequency error to fall below 10% of the step
limit, with a doubled exit threshold; correction saturation clears lock.
The reported effective bandwidth is a configured ceiling. Capture cadence
and correction limits also determine actual convergence.

Automatic LO updates use the raw-count drift fit, before the next DMA starts.
The next capture overlaps phase snapshot conversion and spectrum calculation.
The existing startup prefix is skipped to allow the pipeline to settle. Queued
DMA setting changes receive the next lock handoff; toggling tracking or changing
the total bound can still
wait for the current transfer. Phase-noise averaging continues through small
automatic corrections; manual retunes, failures and acquisition changes clear
results and lock history. Jitter uses the LO that acquired the spectrum.

Tracking can suppress frequency fluctuations near its loop bandwidth. Disable
it when studying those fluctuations. Lock indicates frequency convergence;
it does not establish carrier presence or calibrate phase-noise accuracy.
Board validation of tracking direction, settling and noise-floor effects is
still pending. The numerical regressions retain known PM while converging
with both positive and negative carrier offsets.

The LO fields and CSV LO metadata show nominal settings. Applied frequencies
remain available through `get_parameters()`; `get_tracking_parameters()` returns
enabled, requested/effective bandwidth, step/total bounds, nominal LO0/LO1,
applied corrections 0/1, measured LO-minus-input offsets 0/1, and lock flags 0/1.
Frequency values are double-precision Hz. Settings and spectra are separate
RPC reads and can differ during a configuration change.

```python
analyzer.set_tracking_bandwidth(0.1)
analyzer.set_tracking_max_step(0.05)
analyzer.set_tracking_max_correction(100.0)
analyzer.set_tracking_enabled(True)
print(analyzer.get_tracking_parameters())
# Restore both manually configured LO frequencies:
analyzer.set_tracking_enabled(False)
```

## Validation

Run the software integration regressions (Docker C++/web images and a Python
environment with the Koheron client, NumPy and SciPy):

```sh
bash examples/alpha250/phase-noise-analyzer/tests/run.sh
```

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

All numeric web controls use selected-digit tuning by default, including local oscillators, acquisition settings, delay, and DAC angles, duty cycle and seed. Click a digit and scroll anywhere while the input retains focus; Left/Right selects its place, Up/Down tunes it. F2 or Ctrl+A selects the complete value for keyboard entry; Enter applies and Escape cancels. Integer fields reject fractions and out-of-range values. Local-oscillator display units can be changed without writing hardware.
