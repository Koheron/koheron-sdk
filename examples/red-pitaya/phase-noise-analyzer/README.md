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
Each DMA transfer contains 262144 signed phase samples. The server processes
65536 samples with a 32768-point Hann Welch estimator, yielding 16385 bins in
rad²/Hz. The plotted single-sideband phase noise is `10 log10(S_phi / 2)` dBc/Hz.
Changing acquisition settings discards settling data and clears averages.

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
spectra in `tmp/tests/red-pitaya-phase-noise-analyzer/loopback.npz` and checks
agreement within 5%. It leaves a 10 MHz unmodulated DAC0 carrier for the display.
The PM client is reused from `examples/alpha250/phase-modulator` and reads the
125 MHz clock from the board.

Carrier power uses the nominal ±1 V ADC range and a 50 ohm equivalent load;
it is an estimate without per-board gain/frequency calibration. The multiplier
scaling follows [AMD PG104](https://docs.amd.com/r/en-US/pg104-cmpy/Output-Product-Range).
A DAC/ADC loopback shares the board clock and measures residual path noise;
an external carrier is needed to characterize independent source phase noise.

## Hardware results

Validated on a Red Pitaya with DAC0 connected to ADC0 (LV): the 0.1 rad PM
tone at 6103.515625 Hz integrated to 0.00506183 rad², a +1.24% error.
Slow tracking reduced initial LO offsets of +0.25 Hz and −0.25 Hz to below
0.02 Hz in about five seconds, and disabling tracking restored the nominal LO.

Vivado 2025.1 implementation passed the configured setup/hold/pulse-width and
bus-skew checks (WNS 0.149 ns, WHS 0.013 ns), with no unconstrained internal
endpoints. The inherited board constraints still omit some external I/O delays.
The design uses 14697 LUTs, 26 block RAMs and 53 DSP slices.
