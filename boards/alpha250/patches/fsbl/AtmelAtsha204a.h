/** \file AtmelAtsha204a.h
* Header file for plain-C atmel_atsha204a functions, used in module tests.
* \author Garry Jeromson
* \date 16.06.2015
*
* Copyright (c) 2015 Enclustra GmbH, Switzerland.
* All rights reserved.
*/

#pragma once


//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "AtmelAtsha204aTypes.h"
#include "StandardIncludes.h"


//-------------------------------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------------------------------

/// The number of milliseconds it takes for the device to come out of sleep mode
#define ATMEL_ATSHA204A_WAKE_TIME_MILLISECONDS (500)

/// Average time for the read command to complete, after which polling should start
#define ATMEL_ATSHA204A_READ_EXECUTION_TIME_MILLISECONDS (1)


//-------------------------------------------------------------------------------------------------
// Global variables
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// Function declarations
//-------------------------------------------------------------------------------------------------

/**
 * \brief Wake the device by setting I2C SDA low for the required time period.
 *
 *	Note that this is achieved by a general call to device address 0x00.
 **
 * @param verifyDeviceType	Set true to verify the device type by reading back a command response
 * 							immediately after wake
 * @return					Result code
 */
EN_RESULT AtmelAtsha204a_Wake(bool verifyDeviceType);


/**
 * \brief Put the device in low-power sleep mode.
 * @return	Result code
 */
EN_RESULT AtmelAtsha204a_Sleep();


/**
 * \brief Encode an address for reading or writing.
 *
 * @param[in] zone				Zone select
 * @param[in] slotIndex			Slot select
 * @param[in] wordOffset		Word offset within the given slot
 * @param[out] encodedAddress	The encoded address
 * @return						Result code
 */
EN_RESULT AtmelAtsha204a_EncodeAddress(EZoneSelect_t zone,
                                       uint8_t slotIndex,
                                       uint8_t wordOffset,
                                       uint16_t* encodedAddress);


/**
 * \brief Read from the device.
 *
 * @param[in] sizeSelect		Size select, 4 or 32 bytes
 * @param[in] zoneSelect		Zone select
 * @param[in] encodedAddress	Encoded address
 * @param[out] pReadData		Buffer to receive read data
 * @return						Result code
 */
EN_RESULT AtmelAtsha204a_Read(EReadSizeSelect_t sizeSelect,
                              EZoneSelect_t zoneSelect,
                              uint16_t encodedAddress,
                              uint8_t* pReadData);
