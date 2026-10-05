# PFFFT

Vendored single-precision transform sources from
https://github.com/marton78/pffft at commit
`aa16fd3db58de4ba5dae8b0438440bb9da46b6fa`.

`pffft.cpp` and `implementation.hpp` build these C sources with the SDK GCC
toolchain. The phase-noise analyzers enable
ARM NEON on their Cortex-A9 targets; host checks use the native SIMD backend.
Plans and aligned work buffers are retained between captures.

The local ARMv7 GCC change uses explicit NEON vector loads/stores in the
real forward radix-2/4 butterflies, complex radix-2/3/4/5 butterflies and real/complex
finalization. It avoids GCC's paired 64-bit VFP memory operations while keeping
the arithmetic and FFT layout unchanged. Other compilers, scalar and host SIMD
backends retain typed memory accesses. Define
`PFFFT_DISABLE_GCC_NEON_MEMORY_ACCESS` to compare the typed-access path.
`server/server.mk` explicitly tracks the vendor sources and headers because
the system-header wrapper excludes them from GCC's `-MMD` dependency output.

`tests/test_transform.cpp` checks small transforms against an independent
double-precision DFT, Parseval energy, native/canonical ordering and in-place
forward/inverse transforms, including the PNA production sizes. It runs with
SIMD and scalar sanitizers in the ALPHA250 PNA test suite and can also be built
with the ARM GCC toolchain for direct NEON hardware checks.

Red Pitaya comparisons with GCC 13 show 3.6–6.0% less time for a 32768-point
native real forward FFT and 9.7–9.8% less for a 30000-point complex FFT.
The shared PNA change applies to Red Pitaya, ALPHA250 and ALPHA250-4; only
Red Pitaya hardware was available. See the
[measurement and deployment notes](../../../examples/red-pitaya/phase-noise-analyzer/tests/hardware-validation.md#gcc-armv7-pffft-memory-access-optimization-2026-10-06).

The upstream license is preserved in `pffft-LICENSE.txt` and shipped in analyzer
instrument archives. The vendored subset provides float FFTs only.
