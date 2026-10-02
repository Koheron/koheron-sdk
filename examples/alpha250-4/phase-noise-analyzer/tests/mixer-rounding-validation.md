The ALPHA250-4 mixer formerly selected CMPY `Random_Rounding`, which adds a
half-count rounding constant and randomizes exact halfway ties. It does not
preserve the conditional mean of arbitrary fractional products. See AMD's
[rounding description](https://docs.amd.com/r/en-US/pg104-cmpy/Rounding).
The corrected mixer retains the full 33-bit product, adds a uniform 17-bit
word separately for I and Q, then shifts by 17 to preserve the carrier scale.
The 24-bit CORDIC, 40-bit phase accumulator and N=16 prefilter are retained.

Validation on 2026-10-02 used a split 10 MHz crystal on ADC0/2 and ALPHA250
DAC0 with 10 MHz, 1 degree sine PM at 10 kHz on ADC1/3. The original roughly
6 cm crystal cables were installed. All base local oscillators were 10.001 MHz,
with tracking enabled, CIC 133 and XY acquisition. Source settings were
monitored throughout and stayed unchanged. Only instrument images changed.
The tested image also contained read-only linear I/Q diagnostics and the DMA
RAM-overlap guard; the Linux DMA reservation had already been made exclusive.
The separate RAM-reservation fix is required for trustworthy captures.

Four consecutive fresh captures compared the old mixer, new mixer, old mixer
again, and new mixer again:

| Image | Fresh windows | Recovered PM | 4.010 kHz auto X/Y, rad²/Hz | 4.010 kHz complex cross, rad²/Hz |
| --- | ---: | ---: | --- | --- |
| Old before | 768 | 1.0000758° | 1.132e−14 / 1.562e−14 | 6.019e−15 + 0.430e−15j |
| New first | 768 | 1.0000840° | 1.114e−14 / 1.124e−14 | 4.295e−15 + 0.175e−15j |
| Old return | 768 | 1.0000755° | 2.207e−14 / 2.092e−14 | 1.458e−14 − 2.142e−15j |
| New final | 1024 | 1.0000588° | 1.156e−14 / 1.172e−14 | 4.189e−15 − 0.052e−15j |

The 4 kHz excess returned with the old image and fell again with the new image.
Subtracting a local median baseline and integrating the ±75 Hz neighborhood,
the old-return excess auto power was 4.800e−13 / 4.393e−13 rad², versus
3.286e−14 / 1.295e−14 rad² for the new-final capture. These estimates are near
the baseline in the new image, so they are not an absolute suppression limit.
The median individual-channel high-offset PSD increased about 0.23 dB, as
expected from added rounding noise; the median real cross PSD was essentially
unchanged. The corrected quantizer changes the coherent error rather than
clipping or discarding negative estimates.

The old-return capture had a negative 401 Hz component at approximately six
estimated real-part standard errors. Neither new capture contained a negative
bin beyond four estimated standard errors from 100 Hz to the valid display
limit of approximately 282 kHz. **The 401 Hz component remains unresolved:**
in the new-final capture its cross spectrum was −5.65e−15 + 1.57e−14j rad²/Hz.
Its real part was only 2.61 estimated standard errors below zero, but the
imaginary component remains. A weaker real part is not evidence that this
lower-offset correlation disappeared. This work establishes the mixer fix and
its 4 kHz improvement, not an absolute noise-floor calibration.

The final capture's known PM tone was −41.18254 dBc, with maximum X/Y phase
difference below 0.007 degrees. Six rate checks at CIC 20, 67, 100, 133, 67 and
133 passed, recovering 1.00005–1.00011 degrees and retaining X/Y alignment.
The analyzer was left on the corrected image, CIC 133, XY, tracking locked;
the source remained at 10 MHz with 1 degree PM at 10 kHz.

RTL checks exhaust all 131,072 rounding words for 24 signed products, including
signs, halfway and whole-count boundaries and actual product extrema. The
conditional mean is checked exactly. The vendor CMPY test checks 1,008 full
complex products, byte-padded component packing and the complete rounding
pipeline. Existing phase-counter and prefilter regressions pass, with finite
balance/reset checks and all 91 pairs of 14 rounding generators.

The diagnostic hardware build passed Vivado 2025.1 timing at 200 MHz: setup
slack +0.159918 ns, hold slack +0.017989 ns, and all 12 bus-skew checks. It used
26,272 LUTs, 35,845 registers and 94 DSPs. Existing board I/O timing reports
still identify 28 inputs and five outputs without delay constraints.
The loaded bitstream SHA-256 was
`3e53937cb4c2f96f9587eb9b61e46f41153ad6c6bced9ef28cc62694d339c738`.
