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
#include "TargetModuleConfig.h"

//-------------------------------------------------------------------------------------------------
// Directives, typedefs and constants
//-------------------------------------------------------------------------------------------------


/**
 * \brief Enum for EEPROM device types, with I2C addresses assigned as values.
 */
typedef enum
{

	/// Maxim DS28CN01
	EEepromDevice_MaximDs28cn01_0 = 0x5C,

	/// Maxim DS28CN01
	EEepromDevice_MaximDs28cn01_1 = 0x50,

	/// Atmel ATSHA204A
	EEepromDevice_AtmelAtsha204a = 0x64,

} EEepromDevice_t;


/// Array of all possible EEPROM device addresses
const uint8_t EEPROM_DEVICE_ADDRESSES[3] = { EEepromDevice_MaximDs28cn01_0, EEepromDevice_MaximDs28cn01_1, EEepromDevice_AtmelAtsha204a };

/// EEPROM device type for the detected EEPROM
EEepromDevice_t g_EepromDeviceType;

/// Communications mode register address
#define DS28CN01_REGISTER_ADDRESS_COMMUNICATION_MODE 0xA8

/// Communication mode register value for I2C mode
#define DS28CN01_REGISTER_VALUE_COMMUNICATION_MODE_I2C 0x00

/// Flag to indicate if config properties have been read.
bool g_configPropertiesRead = false;

/// Module serial number
uint32_t g_moduleSerialNumber;

/// Product number info
ProductNumberInfo_t g_productNumberInfo;

/// Module MAC address 0 (the first of the 2 assigned to each module)
uint64_t g_macAddress;


//-------------------------------------------------------------------------------------------------
// Function definitions
//-------------------------------------------------------------------------------------------------

/**
 * \brief Test if a recognised EEPROM device is present at the given I2C device address.
 *
 * @param eepromI2cAddress			The device address
 * @param[out] pDeviceIsPresent		True if a device is detected
 * @return							Result code
 */
EN_RESULT IsEepromPresentAtDeviceAddress(uint8_t eepromI2cAddress, bool* pDeviceIsPresent)
{
	if (pDeviceIsPresent == NULL)
	{
		return EN_ERROR_NULL_POINTER;
	}

	// Perform any config/wake functions that are required for the device to be detected.
	switch (eepromI2cAddress)
	{
	case EEepromDevice_MaximDs28cn01_0:
	case EEepromDevice_MaximDs28cn01_1:
	{
		// The Maxim DS28CN01 EEPROM needs to be switched into I2C mode by writing a zero to the communication mode
		// register.
		uint8_t communicationModeBuffer = DS28CN01_REGISTER_VALUE_COMMUNICATION_MODE_I2C;
		if (EN_FAILED(I2cWrite(eepromI2cAddress,
				DS28CN01_REGISTER_ADDRESS_COMMUNICATION_MODE,
				EI2cSubAddressMode_OneByte,
				(uint8_t*)&communicationModeBuffer,
				sizeof(communicationModeBuffer))))
		{
			*pDeviceIsPresent = false;
			return EN_SUCCESS;
		}

		// Try to read from address 0 to see if the device responds.
		uint8_t readBuffer;
		if (EN_FAILED(
				I2cRead(eepromI2cAddress, 0, EI2cSubAddressMode_OneByte, sizeof(readBuffer), (uint8_t*)&readBuffer)))
		{
			*pDeviceIsPresent = false;
		}
		else
		{
			*pDeviceIsPresent = true;
		}

		break;
	}
	case EEepromDevice_AtmelAtsha204a:
	{
		// Send the wake token, and verify that the response confirms the device is an Atmel ATSHA204A.
		if (EN_FAILED(AtmelAtsha204a_Wake(true)))
		{
			*pDeviceIsPresent = false;
		}
		else
		{
			*pDeviceIsPresent = true;
			AtmelAtsha204a_Sleep();
		}

		break;
	}
	default:
		break;
	}

	return EN_SUCCESS;
}


/**
 * \brief Determine which type of EEPROM device is on the module.
 *
 * @return Result code
 */
EN_RESULT DetermineEepromType()
{
	unsigned int addressCount = sizeof(EEPROM_DEVICE_ADDRESSES) / sizeof(EEPROM_DEVICE_ADDRESSES[0]);
	unsigned int addressIndex = 0;
	for (addressIndex = 0; addressIndex < addressCount; addressIndex++)
	{
		bool devicePresentAtI2cAddress = false;
		uint8_t deviceAddress = EEPROM_DEVICE_ADDRESSES[addressIndex];
		EN_RETURN_IF_FAILED(IsEepromPresentAtDeviceAddress(deviceAddress, &devicePresentAtI2cAddress));

		if (devicePresentAtI2cAddress)
		{
			g_EepromDeviceType = (EEepromDevice_t)deviceAddress;
			break;
		}
	}

	return EN_SUCCESS;
}


EN_RESULT Eeprom_Initialise()
{
	EN_RETURN_IF_FAILED(DetermineEepromType());

	return EN_SUCCESS;
}


EN_RESULT Eeprom_ReadBasicModuleInfo()
{
	switch (g_EepromDeviceType)
			{
			case EEepromDevice_MaximDs28cn01_0:
			case EEepromDevice_MaximDs28cn01_1:
			{
				// Get the serial number
				uint8_t readBuffer[4];
				EN_RETURN_IF_FAILED(I2cRead(g_EepromDeviceType,
						MODULE_INFO_ADDRESS_SERIAL_NUMBER,
						EI2cSubAddressMode_OneByte,
						4,
						(uint8_t*)&readBuffer));

				g_moduleSerialNumber = ByteArrayToUnsignedInt32((uint8_t*)&readBuffer);

				// Product number
				EN_RETURN_IF_FAILED(I2cRead(g_EepromDeviceType,
						MODULE_INFO_ADDRESS_PRODUCT_NUMBER,
						EI2cSubAddressMode_OneByte,
						4,
						(uint8_t*)&readBuffer));

				uint32_t productNumber = ByteArrayToUnsignedInt32((uint8_t*)&readBuffer);
				g_productNumberInfo = ParseProductNumber(productNumber);

				// MAC address
				uint8_t macAddressBuffer[6];
				EN_RETURN_IF_FAILED(I2cRead(g_EepromDeviceType,
						MODULE_INFO_ADDRESS_MAC_ADDRESS,
						EI2cSubAddressMode_OneByte,
						6,
						(uint8_t*)&macAddressBuffer));

				g_macAddress = ByteArrayToUnsignedInt64((uint8_t*)&macAddressBuffer);

				break;
			}
			case EEepromDevice_AtmelAtsha204a:
			{
				uint8_t readBuffer[4];
				uint16_t encodedAddress = 0;

				// Config data is stored in slot 0 of the OTP zone.
				uint8_t slotIndex = 0;

				// Serial number
				uint8_t serialNumberWordOffset = (MODULE_INFO_ADDRESS_SERIAL_NUMBER / 4);

		#if _DEBUG == 1
				EN_PRINTF("Reading module serial number..\r\n");
		#endif

				EN_RETURN_IF_FAILED(
						AtmelAtsha204a_EncodeAddress(EZoneSelect_Otp, slotIndex, serialNumberWordOffset, &encodedAddress));

				EN_RETURN_IF_FAILED(
						AtmelAtsha204a_Read(EReadSizeSelect_4Bytes, EZoneSelect_Otp, encodedAddress, (uint8_t*)&readBuffer));

				g_moduleSerialNumber = ByteArrayToUnsignedInt32((uint8_t*)&readBuffer);

		#if _DEBUG == 1
				EN_PRINTF("Serial number = %d\r\n", g_moduleSerialNumber);
		#endif

				// Product number
		#if _DEBUG == 1
				EN_PRINTF("Reading module product number..\r\n");
		#endif
				uint8_t productNumberWordOffset = (MODULE_INFO_ADDRESS_PRODUCT_NUMBER / 4);

				EN_RETURN_IF_FAILED(
						AtmelAtsha204a_EncodeAddress(EZoneSelect_Otp, slotIndex, productNumberWordOffset, &encodedAddress));

				EN_RETURN_IF_FAILED(
						AtmelAtsha204a_Read(EReadSizeSelect_4Bytes, EZoneSelect_Otp, encodedAddress, (uint8_t*)&readBuffer));

				uint32_t productNumber = ByteArrayToUnsignedInt32((uint8_t*)&readBuffer);
				g_productNumberInfo = ParseProductNumber(productNumber);

		#if _DEBUG == 1
				EN_PRINTF("Product number = 0x%x\r\n", productNumber);
		#endif


		#if _DEBUG == 1
				EN_PRINTF("Reading module MAC address..\r\n");
		#endif

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

				break;
			}
			default:
				break;
			}

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


/**
 * \brief Read the module config info from EEPROM.
 *
 * @param configDataSizeBytes	The number of bytes to read
 * @param[out] pConfigData		Pointer to buffer to receive the config data
 * @return						Result code
 */
EN_RESULT Eeprom_GetModuleConfigData(uint8_t* pConfigData)
{
	if (pConfigData == NULL)
	{
		return EN_ERROR_NULL_POINTER;
	}

	switch (g_EepromDeviceType)
	{
	case EEepromDevice_MaximDs28cn01_0:
	case EEepromDevice_MaximDs28cn01_1:
	{
		EN_RETURN_IF_FAILED(I2cRead(g_EepromDeviceType,
				CONFIG_PROPERTIES_START_ADDRESS,
				EI2cSubAddressMode_OneByte,
				CONFIG_PROPERTIES_LENGTH_BYTES,
				pConfigData));
		break;
	}
	case EEepromDevice_AtmelAtsha204a:
	{
		uint8_t required4ByteReads = DivideRoundUp(CONFIG_PROPERTIES_LENGTH_BYTES, 4);
		uint8_t transferBytesRemaining = CONFIG_PROPERTIES_LENGTH_BYTES;

		uint8_t readIndex = 0;
		for (readIndex = 0; readIndex < required4ByteReads; readIndex++)
		{
			uint16_t encodedAddress = 0;
			uint8_t configDataWordOffset = (CONFIG_PROPERTIES_START_ADDRESS / 4) + readIndex;

			EN_RETURN_IF_FAILED(
					AtmelAtsha204a_EncodeAddress(EZoneSelect_Otp, 0, configDataWordOffset, &encodedAddress));

			uint8_t readBuffer[4];
			EN_RETURN_IF_FAILED(
					AtmelAtsha204a_Read(EReadSizeSelect_4Bytes, EZoneSelect_Otp, encodedAddress, (uint8_t*)&readBuffer));

			// Copy the read bytes to the output buffer.
			uint8_t dataBytesToCopy = min(4, transferBytesRemaining);

			uint8_t byteIndex = 0;
			uint8_t byteOffset = readIndex * 4;
			for (byteIndex = 0; byteIndex < dataBytesToCopy; byteIndex++)
			{
				pConfigData[byteOffset + byteIndex] = readBuffer[byteIndex];
			}

			transferBytesRemaining -= 4;
		}

		break;
	}
	default:
		break;
	}

	return EN_SUCCESS;
}


/**
 * \brief Parse a raw byte vector of module configution information to the relevant properties.
 *
 * This function takes an array of ModuleConfigProperty_t structs, which should be selected
 * according to module type, and sets the value and valueAsStored elements according the parsed
 * info from the raw data. The assumption is made that the first byte of the raw byte array is
 * the byte read from the config data start address.
 *
 * @param pRawConfigData			Pointer to array of raw config data
 * @param configDataStartAddress	The config data start address in EEPROM
 * @param configPropertyCount		The number of config properties for the module
 * @param[out] pPropertyArray		Pointer to array of parsed config properties
 */
EN_RESULT ParseByteVectorToModuleConfig(const uint8_t* pRawConfigData)
{
	if (pRawConfigData == NULL)
	{
		return EN_ERROR_NULL_POINTER;
	}

	unsigned int propertyIndex = 0;
	for (propertyIndex = 0; propertyIndex < CONFIG_PROPERTY_COUNT; propertyIndex++)
	{
		ModuleConfigProperty_t* pConfigProperty = &g_pConfigProperties[propertyIndex];

		uint8_t byteIndex = pConfigProperty->address - CONFIG_PROPERTIES_START_ADDRESS;
		uint8_t configByte = pRawConfigData[byteIndex];
		pConfigProperty->valueAsStored =
				ExtractBitRange(configByte, pConfigProperty->endBit, pConfigProperty->startBit);

#if _DEBUG == 1
		EN_PRINTF("   %s: Value = %d\r\n", pConfigProperty->description, pConfigProperty->valueAsStored);
		EN_PRINTF("   %s: Max value = %d\r\n", pConfigProperty->description, pConfigProperty->maxValue);
		EN_PRINTF("   %s: Min value = %d\r\n", pConfigProperty->description, pConfigProperty->minValue);
#endif

		// Check that the value is in range.
		if (pConfigProperty->valueAsStored < pConfigProperty->minValue ||
				pConfigProperty->valueAsStored > pConfigProperty->maxValue)
		{
			EN_PRINTF("   %s: Out of Range - Value equals = 0x%x\r\n", pConfigProperty->description, pConfigProperty->valueAsStored);
			return EN_ERROR_MODULE_CONFIG_PROPERTY_VALUE_OUT_OF_RANGE;
		}

		if (pConfigProperty->resolution != 0)
		{
			pConfigProperty->value = (uint32_t)pConfigProperty->resolution << (pConfigProperty->valueAsStored - 1);
		}
		else
		{
			pConfigProperty->value = pConfigProperty->valueAsStored;
		}
	}

	return EN_SUCCESS;
}


void Eeprom_PrintModuleConfig()
{
	if (g_pConfigProperties == NULL)
	{
		return;
	}

	unsigned int propertyIndex = 0;
	for (propertyIndex = 0; propertyIndex < CONFIG_PROPERTY_COUNT; propertyIndex++)
	{
		const ModuleConfigProperty_t* pConfigProperty = &g_pConfigProperties[propertyIndex];

#if _DEBUG == 1
		EN_PRINTF("   %s: Address = 0x%x\r\n", pConfigProperty->description, pConfigProperty->address);
		EN_PRINTF("   %s: Bit length = %d\r\n", pConfigProperty->description, pConfigProperty->lengthBits);
		EN_PRINTF("   %s: Start bit = %d\r\n", pConfigProperty->description, pConfigProperty->startBit);
		EN_PRINTF("   %s: End bit = %d\r\n", pConfigProperty->description, pConfigProperty->endBit);
		EN_PRINTF("   %s: Max value = %d\r\n", pConfigProperty->description, pConfigProperty->maxValue);
		EN_PRINTF("   %s: Min value = %d\r\n", pConfigProperty->description, pConfigProperty->minValue);
		EN_PRINTF("   %s: Resolution = %d\r\n", pConfigProperty->description, pConfigProperty->resolution);
		EN_PRINTF("   %s: Value as stored = %d\r\n", pConfigProperty->description, pConfigProperty->valueAsStored);
		EN_PRINTF("   %s: Value = %d\r\n", pConfigProperty->description, pConfigProperty->value);
#endif

		if (pConfigProperty->keyValueCount != 0 && pConfigProperty->pValueKey != NULL)
		{
			uint8_t keyValueIndex = 0;
			for (keyValueIndex = 0; keyValueIndex < pConfigProperty->keyValueCount; keyValueIndex++)
			{
				if (pConfigProperty->pValueKey[keyValueIndex].value == pConfigProperty->valueAsStored)
				{
					EN_PRINTF("   %-30s%s\r\n",
						pConfigProperty->description,
						pConfigProperty->pValueKey[keyValueIndex].meaning);
					break;
				}
			}
		}
		else
		{
			EN_PRINTF("   %-30s%d\r\n", pConfigProperty->description, pConfigProperty->value);
		}
	}
}


/**
 * \brief Read and parse module configuration data from the module EEPROM.
 *
 * Note that it is assumed all config data is stored contiguously.
 *
 * @param printConfigInformation	True to print configuration info
 * @return							A result code
 */
EN_RESULT Eeprom_ReadModuleConfig()
{
	uint8_t rawConfigData[CONFIG_PROPERTIES_LENGTH_BYTES];
	EN_RESULT result = Eeprom_GetModuleConfigData((uint8_t*)&rawConfigData);

	if (EN_FAILED(result))
	{
		EN_PRINTF("Failed to read module configuration, error code 0x%x\r\n", result);
		return result;
	}

	EN_RETURN_IF_FAILED(ParseByteVectorToModuleConfig((uint8_t*)&rawConfigData));

	g_configPropertiesRead = true;

	return EN_SUCCESS;
}


EN_RESULT Eeprom_GetModuleConfigProperty(uint8_t propertyIndex, ModuleConfigProperty_t* pConfigProperty)
{
	if (pConfigProperty == NULL)
	{
		return EN_ERROR_NULL_POINTER;
	}

	if (propertyIndex > CONFIG_PROPERTY_COUNT - 1)
	{
		return EN_ERROR_INVALID_MODULE_CONFIG_PROPERTY_INDEX;
	}

	if (!g_configPropertiesRead)
	{
		return EN_ERROR_MODULE_CONFIG_PROPERTIES_NOT_YET_READ;
	}

	pConfigProperty = &g_pConfigProperties[propertyIndex];

	return EN_SUCCESS;
}


EN_RESULT Eeprom_GetModuleConfigPropertyValue(uint8_t propertyIndex, uint32_t* pPropertyValue)
{
	if (pPropertyValue == NULL)
	{
		return EN_ERROR_NULL_POINTER;
	}

	if (propertyIndex > CONFIG_PROPERTY_COUNT - 1)
	{
		return EN_ERROR_INVALID_MODULE_CONFIG_PROPERTY_INDEX;
	}

	if (!g_configPropertiesRead)
	{
		return EN_ERROR_MODULE_CONFIG_PROPERTIES_NOT_YET_READ;
	}

	*pPropertyValue = g_pConfigProperties[propertyIndex].value;

	return EN_SUCCESS;
}


EN_RESULT Eeprom_GetModuleConfigPropertyDescription(uint8_t propertyIndex, char* pDescription)
{
	if (pDescription == NULL)
	{
		return EN_ERROR_NULL_POINTER;
	}

	if (propertyIndex > CONFIG_PROPERTY_COUNT - 1)
	{
		return EN_ERROR_INVALID_MODULE_CONFIG_PROPERTY_INDEX;
	}

	if (!g_configPropertiesRead)
	{
		return EN_ERROR_MODULE_CONFIG_PROPERTIES_NOT_YET_READ;
	}

	uint8_t characterIndex = 0;
	for (characterIndex = 0; characterIndex < CONFIGURATION_PROPERTY_NAME_MAXLENGTH_CHARACTERS; characterIndex++)
	{
		*(pDescription + characterIndex) = g_pConfigProperties[propertyIndex].description[characterIndex];

		// Break at the null terminator.
		if (g_pConfigProperties[propertyIndex].description[characterIndex] == '\0')
		{
			break;
		}
	}

	return EN_SUCCESS;
}

EN_RESULT Eeprom_GetModuleConfigPropertKeyedValue(uint8_t propertyIndex, char* pKeyedValue)
{
	if (pKeyedValue == NULL)
	{
		return EN_ERROR_NULL_POINTER;
	}

	if (propertyIndex > CONFIG_PROPERTY_COUNT - 1)
	{
		return EN_ERROR_INVALID_MODULE_CONFIG_PROPERTY_INDEX;
	}

	if (!g_configPropertiesRead)
	{
		return EN_ERROR_MODULE_CONFIG_PROPERTIES_NOT_YET_READ;
	}

	ModuleConfigProperty_t* pConfigProperty = &g_pConfigProperties[propertyIndex];

	if (pConfigProperty->keyValueCount != 0 && pConfigProperty->pValueKey != NULL)
	{
		uint8_t keyValueIndex = 0;
		for (keyValueIndex = 0; keyValueIndex < pConfigProperty->keyValueCount; keyValueIndex++)
		{
			if (pConfigProperty->pValueKey[keyValueIndex].value == pConfigProperty->valueAsStored)
			{
				uint8_t characterIndex = 0;
				for (characterIndex = 0; characterIndex < CONFIGURATION_PROPERTY_NAME_MAXLENGTH_CHARACTERS; characterIndex++)
				{
					*(pKeyedValue + characterIndex) = pConfigProperty->pValueKey[keyValueIndex].meaning[characterIndex];

					// Break at the null terminator.
					if (pConfigProperty->pValueKey[keyValueIndex].meaning[characterIndex] == '\0')
					{
						break;
					}
				}

				break;
			}
		}
	}
	else
	{
		return EN_ERROR_MODULE_CONFIG_PROPERTY_DOES_NOT_HAVE_VALUE_KEY;
	}

	return EN_SUCCESS;

}


EN_RESULT Eeprom_Read()
{
	EN_RETURN_IF_FAILED(Eeprom_ReadBasicModuleInfo());

	EN_RETURN_IF_FAILED(Eeprom_ReadModuleConfig());

	return EN_SUCCESS;
}
