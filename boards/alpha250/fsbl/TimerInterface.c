/** \file TimerFunctions.c
 * Implementation file for timer functions.
 * \author Garry Jeromson
 * \date 22.06.2015
 *
 * Copyright (c) 2015 Enclustra GmbH, Switzerland.
 * All rights reserved.
 */

//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "TimerInterface.h"
#include "TimerInterfaceVariables.h"
#include "sleep.h"

//-------------------------------------------------------------------------------------------------
// Global variable definitions
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
// Function definitions
//-------------------------------------------------------------------------------------------------

EN_RESULT InitialiseTimer()
{
    return EN_SUCCESS;
}

void SleepMilliseconds(uint32_t milliseconds)
{
    usleep(1000 * milliseconds);
}
