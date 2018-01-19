/** \file I2cInterface.h
 * \brief Header file for generic I2C interface functions.
 * \author Garry Jeromson
 * \date 18.06.15
 */

#pragma once

//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "StandardIncludes.h"


//-------------------------------------------------------------------------------------------------
// Types, definitions and constants
//-------------------------------------------------------------------------------------------------

/**
* \brief I2C subaddress modes.
*/
typedef enum
{
    EI2cSubAddressMode_None,    ///< No subaddress
    EI2cSubAddressMode_OneByte, ///< One-byte subaddress
    EI2cSubAddressMode_TwoBytes ///< Two-byte subaddress
} EI2cSubAddressMode_t;




//-------------------------------------------------------------------------------------------------
// Function definitions
//-------------------------------------------------------------------------------------------------

/**
 * \brief Perform any required initialisation for I2C operations.
 *
 * @return		Result code
 */
EN_RESULT InitialiseI2cInterface();


/**
 * \brief Perform a read from the I2C bus.
 *
 * \param[in]	deviceAddress			The device address
 * \param[in]	subAddress				Register subaddress
 * \param[in]	subAddressMode			Subaddress mode
 * \param[in]	numberOfBytesToRead		The number of bytes to read
 * \param[out]	pReadBuffer				Buffer to receive read data
 * \returns								Result code
 */
EN_RESULT I2cRead(uint8_t deviceAddress,
                  uint16_t subAddress,
                  EI2cSubAddressMode_t subAddressMode,
                  uint32_t numberOfBytesToRead,
                  uint8_t* pReadBuffer);

/**
 * \brief Perform a write to the I2C bus.
 *
 * \param	deviceAddress			Device address
 * \param	subAddress				Register subaddress
 * \param	subAddressMode			Subaddress mode
 * \param	writeBuffer				Buffer containing write data
 * \param	numberOfBytesToWrite	The number of bytes to write
 * \returns							Result code
 */
EN_RESULT I2cWrite(uint8_t deviceAddress,
                   uint16_t subAddress,
                   EI2cSubAddressMode_t subAddressMode,
                   const uint8_t* pWriteBuffer,
                   uint32_t numberOfBytesToWrite);
