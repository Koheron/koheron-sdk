// Compile the unmodified upstream C sources with the SDK C++ toolchain.
// Keep third-party C extensions and warnings separate from SDK warnings.
#pragma GCC system_header
#define PFFFT_ENABLE_NEON
#include "pffft.c"
#include "pffft_common.c"
