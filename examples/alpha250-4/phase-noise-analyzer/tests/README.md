These regressions run without a board. They exercise the production averager,
DMA reader and DDS driver, using an in-memory hardware backend for DMA and DDS.
The C++ tests run with AddressSanitizer and UndefinedBehaviorSanitizer. The web
test decodes payloads emitted by the production C++ serializer through the
compiled TypeScript driver.

Build the web assets, then run from this repository:

```sh
make web CFG=examples/alpha250-4/phase-noise-analyzer/config.mk
examples/alpha250-4/phase-noise-analyzer/tests/run.sh
```

The runner expects `.venv/bin/python3` with NumPy, SciPy and the Python client
requirements, plus the `cross-armhf:24.04` and `koheron-web:node20` Docker images.
`PNA_PYTHON`, `PNA_CPP_IMAGE` and `PNA_WEB_IMAGE` can override these defaults. The
C++ image must also provide native `g++-13`, since these tests run on the host CPU.
Browser workspace tests use TypeScript 5.6.3 and jsdom 26.1.0, installed on first
run into the ignored `tmp/tests` dependency directory.

Coverage includes:

- Averaging-window growth, shrinkage and clear operations against a deque oracle.
- Fresh, disjoint acquisition windows and detection of overwritten ring data.
- Autonomous cyclic DMA copying across the ring boundary, synchronized X/Y data, producer progress while the reader pauses, cancellation, a concurrent reader during configuration epoch restart, and per-packet precision/gap metadata.
- Fractional phase scaling above and below unity, including sub-hertz carrier offsets.
- CIC gain compensation at power-of-two and arbitrary rates.
- Retained FIR outputs against the original filter for impulses, ramps, noise and tones; cached response corrections across sample rates.
- Phase conversion with large unwrap offsets, safe integer subtraction, and slope estimation with noise.
- Linear phase-drift rejection, spectral tone preservation, and the near-carrier detrending response.
- Tracking lock detection through acquisition, noisy blocks, sustained detuning, and reset.
- Shared INI settings parsing preserves trimmed storage for bool, integer and floating values, including saved tracking settings and long decimal strings, under AddressSanitizer.
- Shared DMA API compatibility, successful completion, timeout and error status.
- DDS frequency precision, concurrent reads/writes, and nonfinite input rejection.
- Python command dispatch, cross-correlation selection, complete phase arrays and frequency axes.
- Cached single-window FFTs against the previous estimator, including signed CSD, six decades of channel ratio, sample-rate changes, DC and even/odd endpoints.
- Signed-spectrum smoothing, first-valid-bin boundaries, retained raw spectra and frequency-noise conversion.
- Web measurement and tracking decoders against C++ serialized quantities.
- Browser signed smoothing/table values, magnitude display with negative markers, negative-only reference capture, retained reference frequency axes, duplicate-frame accounting, disposal, and signed CSV exports with four LO frequencies and cumulative metadata.
- All four digit-editable LO controls retain sub-hertz settings; CIC controls reject fractional rates and XY displays cumulative progress.
- Plot decimation preserves frequency order, extrema and gaps, while compressing dense finite traces.
- The PNA draw call uses the shared V1 renderer without confusing smoothing labels with the FFT peak flag; negative markers keep their point styling and decimated buffers remain independent.

See [hardware validation notes](hardware-validation.md) for completed board
checks. Absolute noise-floor calibration, FIFO behavior during live CIC/LO
changes, and longer tracking stability tests remain open.

The calculation audit uses the production helpers in `phase-spectrum.hpp`.
`check_calculations.py` compares complex cross spectra against an independent
SciPy implementation of detrending, FIR filtering, symmetric Hann windows,
one-sided density normalization, filter compensation and frequency stitching.
Known modulation tones cover all three stitched segments. Identical signals,
opposite signals, quadrature, independent noise and affine drift exercise sign
and power conventions. `test_phase_spectrum.cpp` also verifies signed cumulative
averaging and cancellation through the actual production helper.

For an optional read-only capture, provide an NPZ with `phase[frames,2,32000]`
and the instrument `parameters` tuple as the first script argument. The audit
then processes identical captured samples with and without phase detrending.

The separate FPGA block simulation uses Vivado's `xvlog`, `xelab` and `xsim`:

```sh
PNA_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh \
    bash examples/alpha250-4/phase-noise-analyzer/tests/run-fpga.sh
```

It checks the actual signed four-sample boxcar arithmetic with extreme and
random inputs, and paired CIC control under asymmetric downstream stalls.
The paired-control regression checks accepted ADC-clock sample identities,
decimated output timestamps, reset duration and simultaneous configuration
epochs; a negative control reproduces unequal sample counts with the former
always-valid inputs.

For a board alignment regression, split one phase-modulated 10 MHz AWG output
into IN1/IN3 and one reference into IN0/IN2. Set sinusoidal PM to 1° peak at
10 kHz, then run:

```sh
.venv/bin/python3 examples/alpha250-4/phase-noise-analyzer/tests/check_phase_alignment.py 192.168.1.12
```

This measures X/Y phase fits and the signed live cross-spectrum tone over
multiple CIC changes, including repeated rates. It checks phase alignment
within 1°, phase amplitude within 2%, and integrated cross-spectrum power
within 4%. It restores the previous acquisition rate, selected channel and
moving-average count. `--modulation-hz`, `--peak-deg`, `--cic-rates` and
`--output` allow a different known modulation and result location. Repeat the
check after reloading the instrument to cover startup alignment.

The other block regressions check the signed four-sample boxcar with extreme and
random inputs, phase wrapping in both directions, and the two-cycle unwrap
latency. It also confirms that four default LFSRs produce identical rounding
control sequences over 20,000 clocks. That last check documents the legacy-default
shared sequence; it does not demonstrate statistically independent rounding.
Two simulator compatibility fixes move register declarations before initial
assignments and avoid naming a generate block after its parameter. Neither
changes the arithmetic or the legacy generated sequence.

The new prefilter test compares the actual RTL to direct integer convolution
of four 16-sample rectangular kernels. Signed extremes, impulses, alternating
values and random inputs exercise all stages without intermediate truncation.
The reference includes the exact sampled random rounding value and pipeline
delay. Four separately seeded 64-bit XOR LFSRs use taps 64,63,61,60 from
AMD/Xilinx XAPP052. Tests check balance and all six pair correlations over
100,000 clocks, nonzero states and deterministic reset. These are finite
sequence checks, not proof of statistical independence at arbitrary lags.
The shared LFSR's defaults preserve other instruments' previous recurrence.

`test_prefilter_response.py` verifies unity gain, symmetry, 20 MHz rejection
and known small phase modulation through an ideal real mixer, the new filter
and atan at 100 kHz, 500 kHz and 1 MHz. It excludes vendor IP quantization.

This does not simulate the vendor DDS, multiplier, CORDIC, CIC, FIR or complete
DMA path. The initial FPGA review found three measurement concerns:

- The mixer rounding controls are identical across channels. Their effect on
  the measured cross spectrum requires testing; shared control alone does not
  prove correlated residual errors after channel subtraction. The instrument
  now selects distinct seeds and the 64-bit recurrence; the original shared
  sequence is retained only by the legacy-default regression.
- At 200 MS/s and a 10 MHz carrier, the four-point average attenuates the
  20 MHz mixing image by only 2.277 dB before nonlinear phase extraction.
  An ideal floating-point mixer model produces about 49 degrees of sampled
  phase ripple there. This is not a prediction of the final filtered noise
  floor; vendor quantization and the complete filtering chain are excluded.
  The new filter replaces this with four full-precision 16-sample stages,
  giving 57.27 dB of image rejection at 20 MHz.
- The CIC input phase stream advances even when input ready is low. A full
  downstream FIFO can therefore lose ADC-time samples. Independent X/Y
  stalls broke physical alignment on the split-AWG PM check despite correctly
  paired DMA chunks. Paired sample admission and a common CIC/FIR/FIFO reset
  now preserve alignment across rate changes. Shared stalls can still discard
  ADC-time samples; these regressions do not establish lossless sampling under
  arbitrary FIFO pressure.

Precision regressions cover signed round-to-even and saturation at all 0–8
shifts under AXIS stalls. DMA metadata regressions include ring wrap, stale
precision, different X/Y scales, packet overflow and mixed-scale packets.
The paired controller is tested through rate, precision and reset epochs.
The shared web tests exercise the same precision widget on all three boards.

The phase-rounding regression exhausts all 256 fractional codes and all 256
random values at negative, zero and positive phases, including the Pi
boundary. The mean is exactly the 24-bit input in legacy 16-bit phase units.
The headroom regression reproduces the former signed-32 wrap, crosses the same
boundary with the actual 64-bit unwrapper, and tests common-carrier cancellation
plus both differential saturation limits. These tests run in `run-fpga.sh`.

To reproduce the coherent 104th/204th LO harmonics with AMD's bit-accurate
CORDIC model, run:

```sh
.venv/bin/python examples/alpha250-4/phase-noise-analyzer/tests/check_cordic_precision.py
```

This extracts the model from a locally installed Vivado 2025.1 archive into
ignored `tmp/tests`; no vendor files are redistributed. `PNA_VIVADO_PATH`
overrides the install path. At a Cartesian magnitude of 3000 codes, comparable
to the wired experiment, a complete angular sweep verifies that 24-bit output
reduces both harmonics by more than 40 dB relative to 16-bit output. The model
checks phase calculation error relative to the exact quantized I/Q angle;
it does not model ADC noise, stochastic Cartesian rounding, or the full board.
