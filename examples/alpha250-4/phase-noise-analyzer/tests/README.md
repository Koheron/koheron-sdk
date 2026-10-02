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

Coverage includes:

- Averaging-window growth, shrinkage and clear operations against a deque oracle.
- Fresh, disjoint acquisition windows and detection of overwritten ring data.
- Actual DMA copying across the ring boundary, synchronized X/Y data, cancellation, and configuration between complete transfer pairs.
- Fractional phase scaling above and below unity, including sub-hertz carrier offsets.
- CIC gain compensation at power-of-two and arbitrary rates.
- Retained FIR outputs against the original filter for impulses, ramps, noise and tones; cached response corrections across sample rates.
- Phase conversion with large unwrap offsets, positive and negative low-word wraps, and slope estimation with noise.
- Linear phase-drift rejection, spectral tone preservation, and the near-carrier detrending response.
- Tracking lock detection through acquisition, noisy blocks, sustained detuning, and reset.
- Shared INI settings parsing preserves trimmed storage for bool, integer and floating values, including saved tracking settings and long decimal strings, under AddressSanitizer.
- Shared DMA API compatibility, successful completion, timeout and error status.
- DDS frequency precision, concurrent reads/writes, and nonfinite input rejection.
- Python command dispatch, cross-correlation selection, complete phase arrays and frequency axes.
- Signed-spectrum smoothing, first-valid-bin boundaries, retained raw spectra and frequency-noise conversion.
- Web measurement and tracking decoders against C++ serialized quantities.
- Browser signed smoothing/table values, magnitude display with negative markers, duplicate polling prevention, and signed CSV exports with DDS metadata.
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
delay, for both 16-bit and 24-bit I/Q outputs. Six separately seeded 64-bit
XOR LFSRs use taps 64,63,61,60 from AMD/Xilinx XAPP052. Tests check balance
and all 15 pair correlations over
100,000 clocks, nonzero states and deterministic reset. These are finite
sequence checks, not proof of statistical independence at arbitrary lags.
The shared LFSR's defaults preserve other instruments' previous recurrence.

The phase-count rounding test exhausts all 256 random words at every signed
fraction from -512 to 511 counts. It checks the exact mean, signed extremes,
both modulo boundaries and reset. The unwrap test covers 16-bit/32-bit and
24-bit/40-bit configurations with the same physical phase trajectory.

An optional vendor C-model check evaluates the mean angle error after unbiased
Cartesian rounding. It compares the actual 16-bit and 24-bit CORDIC algorithms
at the same physical I/Q amplitude, including all four possible rounding
outcomes at each angle. It writes two 16384-element arrays of doubles (old and
new expected errors in radians) for Fourier analysis. This is a deterministic
extraction-bias check, not a complete instrument noise-floor model.

Extract Vivado's `cordic_v6_0_bitacc_cmodel_lin64.zip` into a temporary directory,
then compile and run using its headers and libraries:

```sh
pna_model=/tmp/pna-cordic-model
g++ -O2 -std=c++17 -I"$pna_model" \
    examples/alpha250-4/phase-noise-analyzer/tests/check_cordic_bias.cpp \
    -L"$pna_model" -lIp_cordic_v6_0_bitacc_cmodel -l:libgmp.so.11 \
    -o "$pna_model/check-cordic-bias"
LD_LIBRARY_PATH="$pna_model" "$pna_model/check-cordic-bias" \
    "$pna_model/angle-bias.bin"
```

`test_mixer_round.sv` exhausts all 131,072 random words for 24 signed full
products, including both product extrema, zero, and values around half and
whole output counts. The sum of all outputs must equal the input product,
which checks the conditional mean exactly rather than with a statistical
tolerance. It also checks adjacent output counts, sign extension and reset.
The existing phase-rounding regression retains modulo-counter boundary checks.

`test_prefilter_response.py` verifies unity gain, symmetry, 20 MHz rejection
and known small phase modulation through an ideal real mixer, the new filter
and atan at 100 kHz, 500 kHz and 1 MHz. It excludes vendor IP quantization.

After generating the FPGA project, the separate optional vendor mixer test uses
the actual full-product CMPY simulation wrapper (Vivado 2025.1):

```sh
PNA_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh \
    bash examples/alpha250-4/phase-noise-analyzer/tests/run-mixer-vendor.sh
```

It checks 1,008 complex products, including signed input extrema, against wide
integer multiplication. Both byte-padded components then pass through the
actual 33-to-16-bit rounding RTL. The test verifies packing, sign extension,
scaling and delivery of every product and rounded output. The model path can
be overridden with `PNA_VENDOR_MIXER_MODEL` for another build layout.

These tests do not simulate the vendor DDS, CORDIC, CIC, FIR or complete
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

The full-product mixer hardware comparison and its remaining low-offset
limit are recorded in [mixer rounding validation](mixer-rounding-validation.md).
