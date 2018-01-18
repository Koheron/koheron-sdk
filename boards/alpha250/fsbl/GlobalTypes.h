
#pragma once

#include "SystemDefinitions.h"

//-------------------------------------------------------------------------------------------------
// Use correct header for types
// This is different for the target platform because uboot implements types in lib/linux/types.h
//-------------------------------------------------------------------------------------------------
#if SYSTEM == XILINX_MICROBLAZE
#include <stdint.h>
#elif SYSTEM == XILINX_ARM_SOC
#include <stdint.h>
#elif SYSTEM == ALTERA_NIOS
#include <stdint.h>


#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif


#elif SYSTEM == ALTERA_ARM_SOC
#include <stdint.h>
#elif SYSTEM == UBOOT
#include <asm/types.h>

typedef s8 int8_t;
typedef u8 uint8_t;

typedef s16 int16_t;
typedef u16 uint16_t;

typedef s32 int32_t;
typedef u32 uint32_t;

typedef s64 int64_t;
typedef u64 uint64_t;

#define true 1
#define false 0
#endif

#ifndef __cplusplus
#define bool char
#define true 1
#define false 0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif
