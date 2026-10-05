# PFFFT

Vendored single-precision transform sources from
https://github.com/marton78/pffft at commit
`aa16fd3db58de4ba5dae8b0438440bb9da46b6fa`.

The upstream files are unchanged. `pffft.cpp` and `implementation.hpp` build
these C sources with the SDK C++ toolchain. The phase-noise analyzers enable
ARM NEON on their Cortex-A9 targets; host checks use the native SIMD backend.
Plans and aligned work buffers are retained between captures.

The upstream license is preserved in `pffft-LICENSE.txt` and shipped in analyzer
instrument archives. The vendored subset provides float FFTs only.
