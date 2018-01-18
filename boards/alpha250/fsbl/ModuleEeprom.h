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


//-------------------------------------------------------------------------------------------------
// EEPROM function declarations
//-------------------------------------------------------------------------------------------------

/**
 * brief Initialise the EEPROM.
 *
 * This function checks possible module EEPROM I2C device address to determine which kind of
 * EEPROM is present. Note that the I2C interface must be initialised before this function is called.
 * @return	Result code
 */
EN_RESULT Eeprom_Initialise();


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



/**
 * \brief Print the module configuration properties.
 */
void Eeprom_PrintModuleConfig();


/**
 * \brief Get the ModuleConfigProperty_t struct containing all information on a given
 * module configuration property.
 *
 * @param propertyIndex				Zero-based property index
 * @param[out] pConfigProperty		Pointer to receive property
 * @return							Result code
 */
EN_RESULT Eeprom_GetModuleConfigProperty(uint8_t propertyIndex, ModuleConfigProperty_t* pConfigProperty);


/**
 * \brief Get the value of a configuration property. For properties which have a resolution, this function
 * will return the interpreted value; for all other properties, the value as stored in EEPROM will be returned.
 *
 * @param propertyIndex 		Zero-based property index
 * @param[out] pPropertyValue	Pointer to receive property value
 * @return						Result code
 */
EN_RESULT Eeprom_GetModuleConfigPropertyValue(uint8_t propertyIndex, uint32_t* pPropertyValue);


/**
 * \brief Get a property description.
 *
 * @param propertyIndex			Zero-based property index
 * @param[out] pDescription		Pointer to receive description
 * @return						Result code
 */
EN_RESULT Eeprom_GetModuleConfigPropertyDescription(uint8_t propertyIndex, char* pDescription);


/**
 * \brief Get a property keyed value. Valid only for properties which have key values (e.g.
 * a stored value of 0 in the "Temperature grade" property has the keyed value "Commercial".
 * For properties which do not have value key, an empty string is returned.
 *
 * @param propertyIndex			Zero-based property index
 * @param[out] pKeyedValue		Pointer to receive keyed value string
 * @return						Result code
 */
EN_RESULT Eeprom_GetModuleConfigPropertKeyedValue(uint8_t propertyIndex, char* pKeyedValue);

