#pragma once

#include "xil_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct metal_io_region;
void spi_port_bind(struct metal_io_region *io);
u32 Xil_In32(UINTPTR address);
void Xil_Out32(UINTPTR address, u32 value);

#ifdef __cplusplus
}
#endif
