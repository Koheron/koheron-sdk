# Red Pitaya phase-noise analyzer

A two-input analyzer for the 125 MS/s, 14-bit Red Pitaya (Zynq-7010).
It measures one selected ADC at a time with independent 48-bit local oscillators.
The FPGA uses the improved four-stage mixer prefilter, CORDIC phase extraction,
phase unwrapping, programmable six-stage CIC and compensation FIR from ALPHA250.
The server shares its phase conversion, drift removal, Welch spectrum, averaging,
jitter and optional slow LO tracking with the ALPHA250 analyzer.

The web interface provides phase/frequency noise plots, smoothing, decade
readouts, CSV export and an independent two-channel DAC phase modulator.
Its LO limits and tuning resolution come from the actual sample rate.
The reference clock is fixed; ALPHA250 clock-selection controls are omitted.

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
Each DMA transfer contains 131072 signed phase samples. The server processes
65536 samples with a 32768-point Hann Welch estimator, yielding 16385 bins in
rad²/Hz. The plotted single-sideband phase noise is `10 log10(S_phi / 2)` dBc/Hz.
Changing acquisition settings discards settling data and clears averages.
The analyzed block starts 32768 samples into each packet, leaving ample settling
time while halving the unused capture data from the original design.

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

These are filtered output steps; the CORDIC itself remains at π/8192 rad.
Higher precision reduces the available output range by the same factor. The
unwrapper resets between acquisitions, so a large LO-to-carrier offset can
overflow at high precision. A packet overflow invalidates its phase, spectrum
and jitter rather than publishing wrapped values. Reduce precision or bring the
LO closer to the carrier to recover. The upstream 32-bit phase accumulator has
a separate limit of approximately ±823550 rad (131072 turns) between resets,
at every output precision. Its overflow also invalidates the entire packet;
bring the LO closer to the carrier in that case. Other CIC rates have different
filter gains.

`get_precision_status()` returns requested/captured extra bits, radians/count,
state (0 settling, 1 valid, 2 overrange, 3 DMA error), valid/overflow/error counts,
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

## Hardware results

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
