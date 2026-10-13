#pragma once

#include "xil_types.h"

#ifdef __cplusplus
extern "C" {
#endif

s32 Xil_WaitForEvent(UINTPTR address, u32 mask, u32 expected, u32 timeout_us);

#ifdef __cplusplus
}
#endif
