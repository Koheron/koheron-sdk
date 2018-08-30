/** \file ModuleEeprom.h
 * \brief Header file for module-independent EEPROM readout functions.
 * \author Garry Jeromson
 * \date 02.07.15
 */

#pragma once


//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "ModuleConfigConstants.h"
#include "StandardIncludes.h"

#define MODULE_INFO_ADDRESS_MAC_ADDRESS 0x10

//-------------------------------------------------------------------------------------------------
// EEPROM function declarations
//-------------------------------------------------------------------------------------------------

/**
 * \brief Read the module EEPROM.
 *
 * This function reads module information from the EEPROM; serial number, product number, MAC
 * address and module configuration.
 * @return
 */
EN_RESULT Eeprom_Read();


/**
 * \brief Read the basic module information from the module EEPROM.
 *
 * Note that this function should be called before calling GetModuleConfigInfo, as the module config
 * info is of different lengths for different modules.
 *
 * @param[out] pSerialNumber	Serial number
 * @param[out] pProductNumber	Product number info
 * @param[out] pMacAddress		MAC address
 * @return						Result code
 */
EN_RESULT Eeprom_GetModuleInfo(uint32_t* pSerialNumber,
		ProductNumberInfo_t* pProductNumberInfo,
		uint64_t* pMacAddress);
