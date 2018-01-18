/** \file I2cInterfaceVariables.h
 * \brief Header file for global variables required for Xilinx I2C interface
 * functions.
 * \author Garry Jeromson
 * \date 07.07.15
 */

#pragma once

//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include <xiicps.h>
#include <xparameters.h>
#include <xparameters_ps.h>

//-------------------------------------------------------------------------------------------------
// Definitions and constants
//-------------------------------------------------------------------------------------------------

/// Device ID
#define IIC_DEVICE_ID XPAR_XIICPS_0_DEVICE_ID

/// Interrupt ID
#define IIC_INTR_ID XPAR_XIICPS_0_INTR

//-------------------------------------------------------------------------------------------------
// Global variable declarations
//-------------------------------------------------------------------------------------------------

extern XIicPs g_XIicPsInstance;
