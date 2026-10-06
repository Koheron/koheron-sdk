The shared estimator now processes 32768-point Hann segments every 16384 samples,
reusing the last three segment spectra for streaming Welch. Each segment receives
its own integer-domain linear detrend. Successive estimates overlap and are
correlated. Existing 65536-sample phase RPCs are retained; raw phase conversion
runs only when a client requests it. `get_stream_status()` returns processed
segments, ring overruns, FFT length, hop length and Welch depth. See the
[shared processing notes](../../../server/drivers/phase-noise/README.md).

# Red Pitaya phase-noise analyzer

The analyzer now uses the [shared PNA plot and atomic spectrum snapshot](../../../server/drivers/phase-noise/README.md).
Captured settings accompany each spectrum; FPS uses its publication sequence.
Existing spectrum and phase RPCs remain available, and the browser supports
older firmware through the existing read path.

Version 1.1.1 ports the ALPHA designs' 24-bit CORDIC phase calculation and
stochastic phase rounding. The legacy radians-per-count scale and
0–8 extra CIC precision bits are retained. This addresses deterministic phase
quantization harmonics; hardware PM/noise-floor validation remains necessary.

A two-input analyzer for the 125 MS/s, 14-bit Red Pitaya (Zynq-7010).
It measures one selected ADC at a time with independent 48-bit local oscillators.
The FPGA uses the improved four-stage mixer prefilter, CORDIC phase extraction,
phase unwrapping, programmable six-stage CIC and compensation FIR from ALPHA250.
The server shares its phase conversion, drift removal, Welch spectrum, averaging,
jitter and optional slow LO tracking with the ALPHA250 analyzer.

The shared extractor retains 24-bit Cartesian mixer and prefilter outputs on
Red Pitaya, ALPHA250 and ALPHA250-4. Rounding mixer products to 16 bits produced
a carrier-dependent weak-PM gain error that wider CORDIC phase output alone
could not remove. The carrier-power status register keeps its existing 16-bit
I/Q units; phase scaling, filter response and DMA formats are unchanged.

The web interface provides phase/frequency noise plots, smoothing, decade
readouts, CSV export and an independent two-channel DAC phase modulator.
Its LO limits and tuning resolution come from the actual sample rate.
The reference clock is fixed; ALPHA250 clock-selection controls are omitted.

## Continuous acquisition

Version 1.2.0 uses continuous cyclic DMA. Hardware acquires while the CPU
processes spectra; settings changes reset phase/filter/FIFO history as one
epoch. Gap, overrange and DMA failures discard the affected spectrum and
restart acquisition. The shared [acquisition documentation](../../../server/drivers/phase-noise/README.md#acquisition-boundaries)
describes the ring, diagnostics and tracking behavior. The 32768-point Welch
estimator, calibration and 0–8-bit phase precision remain unchanged.
Red Pitaya maps CIC arithmetic into DSP slices and shares one 24-bit phase
extractor. ADC and reference muxes select the input and its independent LO
together before demodulation. Channel changes reset the acquisition epoch;
only the selected input is analyzed. Both demod status registers alias the
active extractor, while raw ADC status and DAC generators remain independent.

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

## Build and run

```sh
make CFG=examples/red-pitaya/phase-noise-analyzer/config.mk N_CPUS=4
make CFG=examples/red-pitaya/phase-noise-analyzer/config.mk run HOST=192.168.1.84
```

Open the board's instrument page and select ADC0. Set the nominal LO to the
carrier frequency; this does not change the DAC signal generator. Tracking is
off by default. When enabled it follows only the selected input, with bounded
steps and a bandwidth well below the plotted offset range. Disabling it restores
both nominal LOs. For details see the ALPHA250 analyzer README.

The CIC range is 4–8192. The output rate is `125e6 / (2 * CIC rate)`.
DMA packets contain 8192 signed phase samples in a 512-packet cyclic ring.
The server selects the latest coherent 65536-sample window for a 32768-point
Hann Welch estimator, yielding 16385 bins in rad²/Hz. The plotted single-sideband
phase noise is `10 log10(S_phi / 2)` dBc/Hz. Acquisition changes reset phase and
filter history, discard warm-up data and clear averages.

## Runtime phase precision

The CIC and FIR retain 40 bits. The final quantizer rounds to even and saturates
into the existing signed 32-bit DMA format. Select 0–8 extra fractional bits in
the web interface or with `set_phase_precision(bits)` in Python. The default is
0, with the original phase scale. Precision is latched at each packet boundary;
the server discards settling packets and packets with a different scale.

At CIC 20, the final phase steps are:

| Extra bits | Phase step | Encoded signed phase range |
| --- | --- | --- |
| 0 | 1.6085 mrad | approximately ±3.454 million rad |
| 4 | 100.531 µrad | approximately ±215889 rad |
| 8 | 6.28319 µrad | approximately ±13493 rad |

These are filtered output steps. The CORDIC calculates phase at π/2²¹ rad,
then stochastically rounds to the legacy π/8192 rad unit before CIC averaging.
Higher precision reduces the available output range by the same factor. The
unwrapper resets at acquisition epoch changes or recovery, so a large LO-to-carrier offset can
overflow at high precision. A packet overflow invalidates its phase, spectrum
and jitter rather than publishing wrapped values. Reduce precision or bring the
LO closer to the carrier to recover. The 64-bit phase accumulator feeds a 32-bit CIC input range guard with
a separate limit of approximately ±823550 rad (131072 turns) between resets,
at every output precision. Its overflow also invalidates the entire packet;
bring the LO closer to the carrier in that case. Other CIC rates have different
filter gains.

`get_precision_status()` returns requested/captured extra bits, radians/count,
state (0 settling, 1 valid, 2 overrange, 3 DMA error, 4 sample gap), valid/overflow/error counts,
processing time and capture period in milliseconds. `get_phase_snapshot()`
returns one coherent capture count, scale and validity flag alongside the phase
array in radians. Existing phase and spectrum commands keep their formats.
Spectra fit drift with exact 64-bit integer sums. Each FFT worker subtracts the
fitted slope and its segment's mean in double precision while preparing the
Hann-windowed float-radian samples. This retains fine phase increments even
with a large carrier offset, and reuses the same fit for tracking. Phase
snapshots retain the original float-radian representation of
the relative phase ramp.

## DMA memory

The boot board overlay must reserve `0x18000000–0x1fffffff` exclusively with
`no-map`. The analyzer uses a 32 MiB window at `0x1e000000`, mapped through
`mem_wc`. Its server refuses to start if this window overlaps System RAM.
Older Red Pitaya images define a reusable CMA pool here; uploading an instrument
cannot fix that boot-time reservation. Install a boot tree built from the current
`boards/red-pitaya/config/board.dtso` and reboot before running the analyzer.

## DAC0 to ADC0 validation

Use ADC0's LV input range. The FPGA uses half of the DAC digital full scale; the analog voltage depends
on the output load. The generator and mixer LOs have independent phase paths, so
injected PM remains measurable through the loopback.

```sh
HOST=192.168.1.84 .venv/bin/python3 examples/red-pitaya/phase-noise-analyzer/test.py
```

The script compares an unmodulated carrier with a 0.1 rad peak sine PM tone at
FFT bin 64. Its expected integrated phase power is 0.005 rad². It saves both
spectra in `tmp/tests/red-pitaya-phase-noise-analyzer/loopback-r*-b*-pm*.npz` and checks
agreement within 5%. It restores the previous analyzer and DAC settings, including
the nominal LOs and tracking state. Set `PHASE_BITS`, `CIC_RATE` and `PM_RADIANS`
to exercise other precision settings, decimation rates and PM amplitudes.
The PM client is reused from `examples/alpha250/phase-modulator` and reads the
125 MHz clock from the board.

Carrier power uses the nominal ±1 V ADC range and a 50 ohm equivalent load;
it is an estimate without per-board gain/frequency calibration. The multiplier
scaling follows [AMD PG104](https://docs.amd.com/r/en-US/pg104-cmpy/Output-Product-Range).
A DAC/ADC loopback shares the board clock and measures residual path noise;
an external carrier is needed to characterize independent source phase noise.

## Continuous DMA FPGA validation (1.2.0)

The latest 24-bit Cartesian shared-extractor build uses 13169 LUTs, 20586
registers, 45.5 block RAM tiles and 63 DSP slices at placement on the Zynq-7010.
Vivado 2025.1 routed setup/hold and bus-skew checks passed (WNS +0.421511 ns, WHS +0.007958 ns;
15 bus-skew constraints checked). There were no unconstrained internal endpoints;
inherited external I/O-delay omissions remain. The block-design assertions
check both ADC/reference selection paths, 24-bit
CORDIC rounding, full-history reset, packet metadata and cyclic SG DMA.
The shared Cartesian precision and quantizer timing improvements also build
on ALPHA250 and ALPHA250-4.
The [continuous DMA hardware results](tests/hardware-validation.md) cover
PM calibration, acquisition cadence, settings changes and the remaining
small-signal error on this image.

## Hardware results before continuous DMA

These measurements and utilization figures predate version 1.2.0. See the
[current hardware results](tests/hardware-validation.md) for continuous DMA.

Validated on a Red Pitaya with DAC0 connected to ADC0 (LV): all nine precision
settings measured the 0.1 rad PM tone at 6103.515625 Hz within 0.16% of the
expected 0.005 rad². At CIC 67 and maximum precision, the error was +0.15%.
Both signs of a 100 kHz LO offset caused reported overrange at +8 bits; reducing
precision recovered acquisition without a DMA error. Rapid precision changes
kept snapshot validity and scale consistent. Both signs of a 4 MHz LO offset
also triggered the upstream accumulator guard at the default precision and
recovered without a DMA error. With maximum precision, a 10 mrad PM tone
measured within 0.6% of expected power at LO offsets of 0 and ±40 kHz.
Slow tracking reduced initial LO offsets of +0.25 Hz and −0.25 Hz to below
0.025 Hz in about five seconds, and disabling tracking restored the nominal LO.

The selectable output step does not establish a calibrated noise floor or
small-signal accuracy. A carrier-phase sweep with the same FPGA image measured
a 1 mrad PM tone with power errors of −29% to +22%; other alignments in earlier
sweeps produced still larger errors. A 10 mrad tone was much more accurate. The
responsible stage has not been isolated; increasing the CORDIC width alone
did not remove the weak-tone bias.

Cached PFFFT plans and aligned buffers use ARM NEON on the Cortex-A9. Combined
with overlapped tracking and DMA, shorter capture packets and reuse of the
integer drift fit, this improves acquisition throughput without
changing the Welch window, overlap, bin spacing or phase-noise density
normalization. PFFFT is vendored with its license in the instrument archive;
no FFT runtime package is required on the board. Detrending and segment mean
removal now run inside each FFT worker, avoiding a separate float array.
Phase snapshots convert a signed difference using an unsigned 32-bit magnitude,
avoiding ARM's software 64-bit-to-float helper without overflowing at the
signed-input endpoints. When tracking is enabled, the next DMA starts after
the drift fit and overlaps snapshot conversion as well as spectral processing.
The sample preparation loop is unrolled fourfold to pipeline independent
Cortex-A9 VFP operations. Its double-precision arithmetic and rounding order
are unchanged; a board benchmark with a large signed-count ramp and small PM
produced identical PSD bins before and after this change.
With tracking off and one spectrum per average, measured rates were:

| CIC rate | Fused preparation baseline | With unrolled preparation |
| --- | --- | --- |
| 4 | 41.1 spectra/s | 44.8 spectra/s |
| 20 | 23.1 spectra/s | 23.1 spectra/s |

These are ten-second measurements on the loopback board with the web page
closed; both baseline comparisons were repeated in the same session.
CPU load at CIC 20 fell from about 69% to 62% across the two CPU cores.
The fastest rate is limited by phase and FFT processing; CIC 20 is
capture-limited. Snapshot RPCs can still wait for processing, now about 21 ms
at CIC 20 instead of 23 ms. No DMA errors or overrange packets were observed
during these benchmarks.
With slow tracking enabled at CIC 20, the analyzer delivered 22.7 spectra/s
with a 0.00181 Hz residual LO error. Five coherent live phase captures agreed
with SciPy's Welch estimator to below 7 parts per million of the peak density
in maximum absolute PSD difference.

Vivado 2025.1 implementation passed the configured setup/hold/pulse-width and
bus-skew checks (WNS 0.175 ns, WHS 0.012 ns), with no unconstrained internal
endpoints. The inherited board constraints still omit some external I/O delays.
At placement the design uses 13122 LUTs, 18734 registers, 26 block RAMs and
53 DSP slices.

Validate the phase-rounding connections after generating the Vivado project:

```sh
source /tools/Xilinx/2025.1/Vivado/settings64.sh
PNA_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh \
    bash examples/red-pitaya/phase-noise-analyzer/tests/run-fpga.sh
.venv/bin/python3 examples/alpha250/phase-noise-analyzer/tests/check_cartesian_precision.py
vivado -mode batch -source examples/red-pitaya/phase-noise-analyzer/tests/check_fpga.tcl \
    -tclargs tmp/examples/red-pitaya/phase-noise-analyzer/fpga/phase-noise-analyzer.xpr
```
