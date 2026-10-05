# ALPHA250 phase-noise analyzer

The analyzer now uses the [shared PNA plot and atomic spectrum snapshot](../../../server/drivers/phase-noise/README.md).
Captured settings accompany each spectrum; FPS uses its publication sequence.
Existing spectrum and phase RPCs remain available, and the browser supports
older firmware through the existing read path.

## Continuous acquisition

Version 1.4.0 uses continuous cyclic DMA. Hardware acquires while the CPU
processes spectra; settings changes reset phase/filter/FIFO history as one
epoch. Gap, overrange and DMA failures discard the affected spectrum and
restart acquisition. The shared [acquisition documentation](../../../server/drivers/phase-noise/README.md#acquisition-boundaries)
describes the ring, diagnostics and tracking behavior. The 32768-point Welch
estimator, calibration and 0–8-bit phase precision remain unchanged.

## Runtime phase precision

Version 1.3.1 calculates 24-bit CORDIC phase and uses a dedicated random stream
to round it without bias into the existing phase unit. This corrects coherent
harmonics introduced by the former deterministic 16-bit phase output. The
downstream 0–8-bit CIC precision settings and radians-per-count scaling are
unchanged; the phase accumulator resets on an acquisition epoch change or recovery.

The acquisition toolbar selects **Standard** or **+1…+8 bits**. The CIC and
compensation FIR retain 40 bits; the packet quantizer rounds to even and
saturates into the 32-bit DMA output. Each extra bit halves radians per count
and the available phase range. Standard retains the previous nominal scale;
+8 gives 256 times finer output steps. This changes quantization, not the
CORDIC resolution or analog noise floor.

`set_phase_precision(bits)` accepts integers 0–8 and returns a boolean. The
choice is stored by **Save settings** (older configurations default to 0).
`get_precision_status()` reports requested/captured precision, radians per
count, state (0 settling, 1 live, 2 overrange, 3 DMA error, 4 sample gap), accepted/overflow/
DMA-error counters and processing/capture times in milliseconds. Saturated
and stale-scale captures clear the current spectrum and do not enter averages
or tracking. Reduce precision or bring the LO closer to the carrier when the
status reports overrange. Integer-domain drift removal preserves the extra
bits before spectral conversion to float.

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

The **DAC signal generator** is open in the right sidebar. Acquisition and
spectrum controls share a compact toolbar; jitter readouts sit below the plot.
Offset readings and laser settings are collapsible sections in the sidebar.
Controls remain disabled until connected, and smoothing starts off.
The generator reads existing settings
when the page loads, supports keyboard/digit/wheel editing, and uses the same
checked `PhaseModulator` RPC as the standalone example. Outputs start muted after
FPGA reset; explicitly prepare and enable the desired signal. Carrier amplitude
is full digital scale; the amplitude field sets phase deviation in degrees.

For a loopback, connect a DAC to its corresponding ADC and match its carrier to
that ADC channel's **Local Oscillator**. PM affects the stimulus only. Choose
modulation frequencies within the acquisition filter's passband. **Save settings** saves the analyzer settings; generator settings are not persisted by it.

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
current six-stage CIC with differential delay 1 and the standard 32-bit DMA scale, the
low-frequency filter correction at CIC rate `R` is:

```text
C(R) = 4 * 2^ceil(log2(R^6)) / R^6
phase_radians = (filtered_DMA_counts - first_count) * C(R) * pi / 8192
```

The factor 4 compensates the FIR's fixed-point scaling: 32 fractional coefficient
bits, a 74-bit accumulator, and a 40-bit output give a DC gain of approximately
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

The sample loop is unrolled fourfold to pipeline independent Cortex-A9 VFP
operations while preserving each sample's arithmetic and rounding order.
The Welch estimator uses a cached PFFFT plan and aligned buffers with ARM NEON
on the Cortex-A9. Two workers process alternate 50%-overlapped segments. The
Hann window and one-sided density normalization, including DC and Nyquist,
are unchanged. PFFFT is vendored with its license in the instrument archive;
no FFT runtime package is required on the board.

Settings and spectral processing share a lock. DMA waits hold neither that
lock nor the DMA settings lock, so even the slowest decimation is cancellable.
Snapshot reads use a short publication lock. Acquisition changes reset phase,
filter and FIFO history as one epoch; warm-up windows are excluded. Failed,
overrange or gapped captures clear published data and averages and restart
acquisition. Invalid phase/PSD are zero and jitter/integration bounds are NaN.
Shutdown cancels a waiting read without completing its acquisition window.

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
original server PSD in rad²/Hz, including DC and Nyquist. Spectrum polling targets
60 reads/s; FPS counts new spectra, measurement readouts at 4 Hz and settings polling at 2 Hz.

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
frequencies and resets the acquisition epoch. Saving analyzer configuration stores
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

Automatic LO updates use the raw-count drift fit after a coherent window is
copied. Each DDS change resets the acquisition epoch, preventing windows from
straddling a retune. Between updates, cyclic DMA overlaps phase conversion and
spectral processing. Queued settings receive the next processing-lock handoff;
they do not wait for a complete slow-decimation window. Averaging continues
through small automatic corrections; manual retunes, failures and acquisition
changes clear results and lock history. Jitter uses the LO that acquired the
spectrum.

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

## Averaging progress

The **Averages** control shows `k/N`: the actual number of spectra in the rolling
window followed by the editable target. For example, `3/90` is still filling and
`90/90` is full. Hover over the count for a description of the current state.
A full window continues updating by replacing its oldest spectrum; it does not
stop acquisition or indicate a completed measurement. The server divides by the
actual count while the window is filling, so early spectra contain fewer averages.
Changing acquisition settings or a failed/invalid capture clears the window.
Changing the target retains the most recent spectra that fit the new window.
The indicator polls the server twice per second and resumes correctly after a
page reload. `get_average_status()` returns `(count, target)` as two uint32 values.

The plot fits the first valid spectrum and keeps the current zoom when a
reference is captured, replaced or cleared. CSV and PNG exports wait for valid
live data. CSV uses the displayed frame's acquisition settings; PNG includes
the trace legend and units on a white background. Both jitter readouts share
the integration band shown beneath the plot. Connection loss marks the view
stale and provides a Retry button.

## Spectrum display and reference trace

The header counts changed spectra displayed per second; repeated cached replies
are excluded. The polling target defaults to 60 reads/s and pauses while the tab
is hidden. Hover over FPS for the measured polling rate and average read,
processing, drawing and scheduling times. Hardware acquisition cadence may be
lower than the polling target, so the noise trace and FPS follow new PSD data.
Spectrum reads return the latest completed result without waiting for the next
FFT calculation. Retuning still clears the published spectrum while settling.
Dense noise traces use short canvas strokes to keep magnified plots responsive.
Logarithmic column reduction retains extrema and gaps; sparse zooms restore all
visible bins. References, capture and CSV retain the full spectrum.

Use **Capture ref** beside the plot to freeze the displayed spectrum, as in
the FFT workspace. The button becomes **Replace ref**; **Clear ref** removes
the overlay. The reference retains the full linear PSD and the displayed
frame's analyzer settings, including its own sample rate. Changing live
decimation does not move the reference's frequency bins. Phase/frequency
selection and the smoothing checkbox apply to both live and reference curves.

CSV export includes a separate reference section with its captured settings,
raw and smoothed display values, and the original PSD in rad²/Hz, including
DC and Nyquist. References last for the current page session; export a CSV
to keep a copy. Saving analyzer configuration does not save the reference.
Settings and spectra are separate RPC reads, so metadata can differ during
a retune. Capture is disabled while the LO is unset, acquisition fails or
the spectrum is empty; an existing reference is retained.

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
Host web checks cover the open DAC panel, checked editing, references and exports,
averaging progress, display cadence, retry, navigation recovery and teardown.
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

## Continuous DMA hardware validation (1.4.0)

On ALPHA250, DAC1 → ADC0 measured a 10 MHz carrier with 10 kHz, 1° peak sine
PM at CIC rates 16, 20, 60 and 100 and extra precision bits 0, 4 and 8. All
twelve measurements were within 1% of the injected phase amplitude after DAC
startup settling. The range endpoints also passed at +8 bits: CIC 4 with
10 kHz PM and CIC 8192 with 300 Hz PM (5.37-second spectrum windows).
DAC0’s generator settings were preserved for its ALPHA250-4 connection.

At CIC 20 with tracking off and one spectrum per average, 20-second runs
produced about 46 spectra/s with no new overrange, DMA errors or sample gaps.
A separate 30-second run with 60 full spectrum reads/s delivered 42.6 fresh
spectra/s. Every frame was valid, the largest fresh-spectrum interval was
40 ms, and no capture-error counter increased. This exercises RPC traffic;
it does not measure browser drawing time.
Pausing the server CPU for one second left FPGA acquisition running: 611
packets completed, exceeding the 512-packet ring. After resume, the analyzer
published a fresh valid PM spectrum without a capture error. A forced S2MM
halt raised the DMA-error counter and automatically recovered in a new epoch.
Slow tracking reduced a +0.25 Hz LO offset to approximately 0.003 Hz in
15 seconds without new capture errors. Switching from CIC 8192 back to 20 recovered without waiting for the
slow window (the synchronous setting/reply round trip took under 10 ms).
These checks establish recovery in these conditions; they do not
guarantee continuous service under every load or exhaustive processing of all
acquired samples.

Vivado 2025.1 routed setup/hold and bus-skew checks passed (WNS +0.047 ns,
WHS +0.004 ns). External I/O-delay omissions in the board constraints remain;
there were no unconstrained internal endpoints.
