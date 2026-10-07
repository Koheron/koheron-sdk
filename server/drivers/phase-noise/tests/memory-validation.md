# PNA capture-buffer and PSD memory optimization

Validated on 2026-10-07. This changes the C++ processing path for ALPHA250,
ALPHA250-4 and Red Pitaya; FPGA logic, calibration, linear fits, FFT length,
hop and averaging semantics are unchanged.

## Build and regression checks

- ARM server builds: all three PNA instruments and the ALPHA250 DPLL monitor,
  using `cross-armhf:24.04` and GCC 13 with the existing strict warning flags.
- Both PNA regression runners: AddressSanitizer/UndefinedBehaviorSanitizer,
  cyclic DMA freshness/overlap/reset/cancellation/error checks, tracking,
  sample-clock changes, configuration, Python and browser regressions.
- New scratch-buffer reuse tests check cleared metadata, retained overlap and
  preservation of the previously published samples for single and paired DMA.
- Fused PSD reduction matches the original clear/accumulate/normalize sequence
  bit for bit for scalar and SIMD layouts, including subnormal output scales.
- The streaming estimator passes its independent SciPy oracle on the host and
  in Cortex-A9 ARMv7 NEON emulation (`qemu-arm-static`). These cover DC/Nyquist,
  rolling means, signed CSD, a 120 dB channel ratio, fine steps on large drift,
  signed integer endpoints and reset behavior. Relative error stays below the
  existing 2 ppm tolerance.
- DPLL passive-monitor host checks and its CDC simulation passed (Vivado
  2026.1). No FPGA implementation build was required for these software edits.

## Hardware checks

ALPHA250 `.105` used 200 MS/s, CIC 100, ADC0, +8 precision and a rolling target
of 100. ALPHA250-4 `.12` used 250 MS/s, CIC 134 and +8 precision; X/Y used a
rolling target of 1 and XY used cumulative averaging. DAC0 on `.105` drives
ADC1/ADC3 on `.12`; ADC0/ADC2 receive the external 10 MHz reference.

Each case warmed up for two seconds, then sampled the server's smoothed
performance counters 24 times at 200 ms intervals while reading complete
spectrum frames. The table reports median total service time, including DMA
copying and publication; it excludes waiting for new samples. The single and
XY baselines average the original-firmware medians before and after the
candidate run. X/Y baselines were taken immediately before the candidate run.

| Instrument / mode | Original ms | Optimized ms | Reduction |
| --- | ---: | ---: | ---: |
| ALPHA250 / ADC0 | 11.257 | 10.339 | 8.2% |
| ALPHA250-4 / XY | 11.556 | 10.736 | 7.1% |
| ALPHA250-4 / X | 10.575 | 9.030 | 14.6% |
| ALPHA250-4 / Y | 10.270 | 8.995 | 12.4% |

Auto-spectrum reduction time fell from about 1.91 to 1.64 ms on ALPHA250 and
from 1.76–1.82 to 1.41 ms on ALPHA250-4. XY retains two independent transforms
and its original complex cross reduction; its gain comes from capture handling.
These are representative measurements under the current board/client load,
not a guaranteed throughput increase at every rate.

All sampled cases maintained 100% unique-sample coverage, with no new DMA errors,
overflow captures or ring overruns during the sampling intervals. Phase RPCs
returned valid snapshots of the expected lengths. The quad's 10 kHz PM power
agreed with the 0.1 degree source setting within 0.07%; candidate/original power
agreed within 0.03% when compared after equivalent restarts. The initial long
running cumulative baseline was excluded from that power comparison.

Temporary candidate packages used the boards' original FPGA payloads and web
assets with only `serverd` replaced. Both original instruments were restored;
their server hashes, sample clocks, precision, nominal LOs and source DAC words
were checked. Temporary installed candidate packages were deleted. Raw captures,
counter samples and the reversible test scripts are local in `tmp/pna-memory/`.
Red Pitaya hardware was not available for this check.
