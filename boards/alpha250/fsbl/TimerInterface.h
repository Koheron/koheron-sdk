/** \file TimerInterface.h
 * \brief Header file for generic timer function declarations.
 * \author Garry Jeromson
 * \date 07.07.15
 */

#pragma once

//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "StandardIncludes.h"


//-------------------------------------------------------------------------------------------------
// Function declarations
//-------------------------------------------------------------------------------------------------

/**
 * \brief Perform any required initialisation for the timer.
 *
 * @returns Result code
 */
EN_RESULT InitialiseTimer();


/**
 * \brief Sleep for the defined number of milliseconds.
 *
 * Note that if the hardware is not working for some reason, this function may get stuck in
 * an infinite loop.
 *
 * @param milliseconds The number of milliseconds to sleep for
 */
void SleepMilliseconds(uint32_t milliseconds);

