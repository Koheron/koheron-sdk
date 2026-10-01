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
#include "AtmelAtsha204a.h"
#include "UtilityFunctions.h"

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
	uint8_t readBuffer[4];
	uint16_t encodedAddress = 0;

	// Config data is stored in slot 0 of the OTP zone.
	uint8_t slotIndex = 0;

	// Serial number
	uint8_t serialNumberWordOffset = (MODULE_INFO_ADDRESS_SERIAL_NUMBER / 4);

	EN_RETURN_IF_FAILED(
			AtmelAtsha204a_EncodeAddress(EZoneSelect_Otp, slotIndex, serialNumberWordOffset, &encodedAddress));

	EN_RETURN_IF_FAILED(
			AtmelAtsha204a_Read(EReadSizeSelect_4Bytes, EZoneSelect_Otp, encodedAddress, (uint8_t*)&readBuffer));

	g_moduleSerialNumber = ByteArrayToUnsignedInt32((uint8_t*)&readBuffer);


	// Product number

	uint8_t productNumberWordOffset = (MODULE_INFO_ADDRESS_PRODUCT_NUMBER / 4);

	EN_RETURN_IF_FAILED(
			AtmelAtsha204a_EncodeAddress(EZoneSelect_Otp, slotIndex, productNumberWordOffset, &encodedAddress));

	EN_RETURN_IF_FAILED(
			AtmelAtsha204a_Read(EReadSizeSelect_4Bytes, EZoneSelect_Otp, encodedAddress, (uint8_t*)&readBuffer));

	uint32_t productNumber = ByteArrayToUnsignedInt32((uint8_t*)&readBuffer);
	g_productNumberInfo = ParseProductNumber(productNumber);

	// MAC address - we need to split this into 2 4-byte reads.
	uint8_t macAddressBuffer1[4];
	uint8_t macAddressBuffer2[4];
	uint8_t macAddressWordOffset = (MODULE_INFO_ADDRESS_MAC_ADDRESS / 4);

	EN_RETURN_IF_FAILED(
			AtmelAtsha204a_EncodeAddress(EZoneSelect_Otp, slotIndex, macAddressWordOffset, &encodedAddress));

	EN_RETURN_IF_FAILED(
			AtmelAtsha204a_Read(EReadSizeSelect_4Bytes, EZoneSelect_Otp, encodedAddress, (uint8_t*)&macAddressBuffer1));

	EN_RETURN_IF_FAILED(
			AtmelAtsha204a_EncodeAddress(EZoneSelect_Otp, slotIndex, macAddressWordOffset + 1, &encodedAddress));

	EN_RETURN_IF_FAILED(
			AtmelAtsha204a_Read(EReadSizeSelect_4Bytes, EZoneSelect_Otp, encodedAddress, (uint8_t*)&macAddressBuffer2));

	uint8_t fullMacAddressBuffer[6];

	unsigned int byteIndex = 0;
	for (byteIndex = 0; byteIndex < 4; byteIndex++)
	{
		fullMacAddressBuffer[byteIndex] = macAddressBuffer1[byteIndex];
	}

	for (byteIndex = 4; byteIndex < 6; byteIndex++)
	{
		fullMacAddressBuffer[byteIndex] = macAddressBuffer2[byteIndex - 4];
	}

	g_macAddress = ByteArrayToUnsignedInt64((uint8_t*)&fullMacAddressBuffer);

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
