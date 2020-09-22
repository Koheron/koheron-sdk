/** \file ModuleConfig.c
 * \brief Implementation file for module-independent EEPROM functions.
 * \author Garry Jeromson
 * \date 02.07.15
 */


//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "ModuleEeprom.h"
#include "I2cInterface.h"
#include "UtilityFunctions.h"

#define EEPROM_DS28CN01_ADDR 0x5C

//-------------------------------------------------------------------------------------------------
// Directives, typedefs and constants
//-------------------------------------------------------------------------------------------------

/// Module serial number
uint32_t g_moduleSerialNumber;

/// Product number info
ProductNumberInfo_t g_productNumberInfo;

/// Module MAC address 0 (the first of the 2 assigned to each module)
uint64_t g_macAddress;


//-------------------------------------------------------------------------------------------------
// Function definitions
//-------------------------------------------------------------------------------------------------

EN_RESULT Eeprom_ReadBasicModuleInfo()
{
	// Get the serial number
	uint8_t readBuffer[4];
	EN_RETURN_IF_FAILED(I2cRead(EEPROM_DS28CN01_ADDR,
			MODULE_INFO_ADDRESS_SERIAL_NUMBER,
			EI2cSubAddressMode_OneByte,
			4,
			(uint8_t*)&readBuffer));

	g_moduleSerialNumber = ByteArrayToUnsignedInt32((uint8_t*)&readBuffer);

	// Product number
	EN_RETURN_IF_FAILED(I2cRead(EEPROM_DS28CN01_ADDR,
			MODULE_INFO_ADDRESS_PRODUCT_NUMBER,
			EI2cSubAddressMode_OneByte,
			4,
			(uint8_t*)&readBuffer));

	uint32_t productNumber = ByteArrayToUnsignedInt32((uint8_t*)&readBuffer);
	g_productNumberInfo = ParseProductNumber(productNumber);

	// MAC address
	uint8_t macAddressBuffer[6];
	EN_RETURN_IF_FAILED(I2cRead(EEPROM_DS28CN01_ADDR,
			MODULE_INFO_ADDRESS_MAC_ADDRESS,
			EI2cSubAddressMode_OneByte,
			6,
			(uint8_t*)&macAddressBuffer));

	g_macAddress = ByteArrayToUnsignedInt64((uint8_t*)&macAddressBuffer);

	return EN_SUCCESS;
}

EN_RESULT Eeprom_GetModuleInfo(uint32_t* pSerialNumber,
		ProductNumberInfo_t* pProductNumberInfo,
		uint64_t* pMacAddress)
{
	if (pSerialNumber == NULL || pProductNumberInfo == NULL || pMacAddress == NULL)
	{
		return EN_ERROR_NULL_POINTER;
	}

	*pSerialNumber = g_moduleSerialNumber;

	*pProductNumberInfo = g_productNumberInfo;

	*pMacAddress = g_macAddress;

	return EN_SUCCESS;
}

EN_RESULT Eeprom_Read()
{
	EN_RETURN_IF_FAILED(Eeprom_ReadBasicModuleInfo());
	return EN_SUCCESS;
}
