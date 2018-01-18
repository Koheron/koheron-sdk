/**
 * \file SystemDefinitions.h
 * \brief This file contains top-level system definitions for module tests and reference designs.
 */

#pragma once


//-------------------------------------------------------------------------------------------------
// System type options
//-------------------------------------------------------------------------------------------------

#define XILINX_MICROBLAZE 0
#define XILINX_ARM_SOC 1
#define ALTERA_NIOS 2
#define ALTERA_ARM_SOC 3
#define UBOOT 4


//-------------------------------------------------------------------------------------------------
// System clock frequency
//-------------------------------------------------------------------------------------------------

#define SYSTEM_CLOCK_FREQUENCY_HZ 100000000

//-------------------------------------------------------------------------------------------------
// Project definitons
//-------------------------------------------------------------------------------------------------

#define MODULE_TESTING 0
#define REFERENCE_DESIGN 1

#define PROJECT MODULE_TEST

//-------------------------------------------------------------------------------------------------
// Printf definitions
//-------------------------------------------------------------------------------------------------

#if SYSTEM == XILINX_MICROBLAZE
#include "xil_printf.h"
#define EN_PRINTF xil_printf
#define EN_FLUSH fflush(stdout)
#elif SYSTEM == XILINX_ARM_SOC
#include "xil_printf.h"
#define EN_FLUSH fflush(stdout)
#define EN_PRINTF xil_printf
#elif SYSTEM == ALTERA_NIOS
#define EN_PRINTF printf
#define EN_FLUSH alt_dcache_flush_all()
#elif SYSTEM == ALTERA_ARM_SOC
#include "uart0_support.h"
#define EN_PRINTF uart0_printf
#define EN_FLUSH fflush(stdout)
#elif SYSTEM == UBOOT
#define EN_PRINTF printf
#define EN_FLUSH fflush(stdout)
#endif


//-------------------------------------------------------------------------------------------------
// Module type definitions
//-------------------------------------------------------------------------------------------------

#define MARS_MX1 0
#define MARS_MX2 1
#define MERCURY_CA1 2
#define MARS_ZX2 3
#define MARS_ZX3 4
#define MARS_AX3 5
#define MERCURY_KX1 6
#define MERCURY_SA1 7
#define MERCURY_ZX1 8
#define MERCURY_ZX5 9
#define MERCURY_SA2 10


//-------------------------------------------------------------------------------------------------
// Base board definitions
//-------------------------------------------------------------------------------------------------

#define MARS_STARTER 0
#define MERCURY_STARTER 1
#define MARS_PM3 2
#define MERCURY_PE1 3
#define MARS_EB1 4

