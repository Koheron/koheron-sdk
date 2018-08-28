/**
 * \file SystemDefinitions.h
 * \brief This file contains top-level system definitions for module tests and reference designs.
 */

#pragma once

//-------------------------------------------------------------------------------------------------
// System clock frequency
//-------------------------------------------------------------------------------------------------

#define SYSTEM_CLOCK_FREQUENCY_HZ 100000000

//-------------------------------------------------------------------------------------------------
// Printf definitions
//-------------------------------------------------------------------------------------------------

#include "xil_printf.h"
#define EN_FLUSH fflush(stdout)
#define EN_PRINTF xil_printf

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