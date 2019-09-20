/** \file InterruptController.h
 * \brief Header file for interrupt controller functions.
 * \author Garry Jeromson
 * \date 24.06.2015
 */

#pragma once


//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "StandardIncludes.h"

#include <xiicps.h>
#include <xparameters.h>
#include <xil_types.h>


//-------------------------------------------------------------------------------------------------
// Global variable declarations
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// Function declarations
//-------------------------------------------------------------------------------------------------

/**
 * \brief Set up the interrupt system.
 *
 * As the interrupt controller is used for different functions (I2C, timer), this function is
 * segregated into its own translation unit. When a block needs to use the interrupt controller,
 * the appropriate calls should be added to the body of this function. Note that the client blocks
 * should be initialised before calling this function.
 *
 * @return Result code
 */
EN_RESULT SetupInterruptSystem();
