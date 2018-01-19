/** \file AtmelAtsha204a.c
 * Implementation file for plain-C Atmel ATSHA204A functions.
 * \author Garry Jeromson
 * \date 16.06.2015
 *
 * Copyright (c) 2015 Enclustra GmbH, Switzerland.
 * All rights reserved.
 */


//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "AtmelAtsha204a.h"
#include "I2cInterface.h"
#include "TimerInterface.h"
#include "UtilityFunctions.h"



//-------------------------------------------------------------------------------------------------
// Directive, typedefs and constants
//-------------------------------------------------------------------------------------------------

/// I2C device address
const uint8_t ATMEL_ATSHA204A_DEVICE_ADDRESS = 0x64;


//-------------------------------------------------------------------------------------------------
// Device organisation
//-------------------------------------------------------------------------------------------------


/// Slot size, in bytes.
const uint8_t SLOT_SIZE_BYTES = 32;

/// Word size, in bytes. This is the base unit for all reads and writes.
const uint8_t WORD_SIZE_BYTES = 4;

/// Maximum word offset per slot
const uint8_t MAX_WORD_OFFSET = 7;

/// Size of the configuration zone, in bytes
const uint8_t CONFIGURATION_ZONE_SIZE_BYTES = 88;

/// Number of slots in the configuration zone
const uint8_t CONFIGURATION_ZONE_SIZE_SLOTS = 3;

/// Slot 3 in the configuration zone is only 24 bytes rather than 32, so the max word offset is limited to 5.
const uint8_t CONFIGURATION_ZONE_SLOT_2_MAX_WORD_OFFSET = 5;

/// Size of the OTP zone, in bytes
const uint8_t OTP_ZONE_SIZE_BYTES = 64;

/// Number of slots in the OTP zone
const uint8_t OTP_ZONE_SIZE_SLOTS = 2;

/// Size of the data zone, in bytes
const uint16_t DATA_ZONE_SIZE_BYTES = 512;

/// Number of slots in the data zone
const uint8_t DATA_ZONE_SIZE_SLOTS = 16;

/// The data slot used for module configuration data
const uint8_t DATA_ZONE_SLOT_MODULE_CONFIGURATION = 0;

/// Byte index of the OTP mode byte within its configuration word.
const uint8_t OTP_MODE_WORD_BYTE_INDEX = 2;


//-------------------------------------------------------------------------------------------------
// Command packets and I/O
//-------------------------------------------------------------------------------------------------

/// Command execution status response block size
const uint8_t STATUS_RESPONSE_BLOCK_SIZE_BYTES = 4;

/// Byte index of count in response block
const uint8_t STATUS_RESPONSE_COUNT_BYTE_INDEX = 0;

/// Byte index of status code in response block
const uint8_t STATUS_RESPONSE_STATUS_BYTE_INDEX = 1;

/// Checksum size
const uint8_t CHECKSUM_LENGTH_BYTES = 2;

/// Index of the count byte in a command packet
const uint8_t COMMAND_PACKET_COUNT_BYTE_INDEX = 0;

/// Size of count in a command packet
const uint8_t COMMAND_PACKET_COUNT_SIZE_BYTES = 1;

/// Index of the opcode byte in a command packet
const uint8_t COMMAND_PACKET_OPCODE_BYTE_INDEX = 1;

/// Size of the opcode byte in a command packet
const uint8_t COMMAND_PACKET_OPCODE_LENGTH_BYTES = 1;

/// Index of param 1 in a command packet
const uint8_t COMMAND_PACKET_PARAM1_BYTE_INDEX = 2;

/// Size of param 1 in a command packet
const uint8_t COMMAND_PACKET_PARAM1_SIZE_BYTES = 1;

/// Index of param 2 in a command packet
const uint8_t COMMAND_PACKET_PARAM2_BYTE_INDEX = 3;

/// Size of param 2 in a command packet
const uint8_t COMMAND_PACKET_PARAM2_SIZE_BYTES = 2;


//-------------------------------------------------------------------------------------------------
// Function declarations
//-------------------------------------------------------------------------------------------------

/**
 *
 * \brief Calculate a CRC-16 used when communicating with the device. Code taken from Atmel's library.
 *
 * The Atmel documentation only specifies that the CRC algorithm used on the ATSHA204A is CRC-16 with polynomial
 * 0x8005; compared to a standard CRC-16, however, the used algorithm doesn't use remainder reflection.
 *
 * @param pData				The data to calculate the CRC for
 * @param dataLengthBytes	The number of bytes to process
 * @return					The CRC
 */
uint16_t AtmelAtsha204a_CalculateCrc(const uint8_t* pData, uint8_t dataLengthBytes)
{
    if (pData == NULL)
    {
        return 0;
    }

    uint8_t counter;
    uint16_t crcRegister = 0;
    uint16_t polynomial = 0x8005;
    uint8_t shiftRegister;
    uint8_t dataBit, crcBit;

    for (counter = 0; counter < dataLengthBytes; counter++)
    {
        for (shiftRegister = 0x01; shiftRegister > 0x00; shiftRegister <<= 1)
        {
            dataBit = (pData[counter] & shiftRegister) ? 1 : 0;
            crcBit = crcRegister >> 15;
            crcRegister <<= 1;
            if (dataBit != crcBit)
            {
                crcRegister ^= polynomial;
            }
        }
    }

    return crcRegister;
}


EN_RESULT AtmelAtsha204a_Wake(bool verifyDeviceIsAtmelAtsha204a)
{
    // The Atmel ATSHA204A needs to be woken up by holding SDA low; we do this by clocking a data byte of 0x00 slow
    // enough such that SDA is low for tWLO (60 us).
    // I2C clock rate = 100kHz
    // Required data bytes to fulfil tWLO: 1 (8 bits, 8 clocks @ 100kHz, 80us)
    uint8_t dummyWriteData = 0x00;

#ifdef _DEBUG
    EN_PRINTF("Attempting to wake Atmel ATSHA204A device...\r\n");
#endif

    I2cWrite(0, 0, EI2cSubAddressMode_OneByte, (uint8_t*)&dummyWriteData, 0);

    // Wait for the device to wake up. 
    SleepMilliseconds(ATMEL_ATSHA204A_WAKE_TIME_MILLISECONDS);

    if (verifyDeviceIsAtmelAtsha204a)
    {
        // Attempt to read the status block.
        uint8_t readBuffer[4];
        EN_RESULT readResult;

        do
        {
            readResult = I2cRead(
                ATMEL_ATSHA204A_DEVICE_ADDRESS, 0, EI2cSubAddressMode_None, sizeof(readBuffer), (uint8_t*)&readBuffer);
        } while (readResult != EN_SUCCESS);


        // At this point, we need to verify the device is actually the EEPROM we think it is.
        // if we attempt a read, we expect a status block of 4 bytes with the After Wake status code
        // (0x11). The block consists of the count, status code, and 2-byte CRC.
        uint8_t expectedStatusBlock[4] = { 0x04, 0x11, 0x33, 0x43 };
        uint32_t expectedResponse = ByteArrayToUnsignedInt32((uint8_t*)&expectedStatusBlock);
        uint32_t actualResponse = ByteArrayToUnsignedInt32((uint8_t*)&readBuffer);

        if (expectedResponse != actualResponse)
        {
            EN_PRINTF("Received response code %x %x %x %x when trying to wake Atmel ATSHA204A \r\n",
                      readBuffer[0],
                      readBuffer[1],
                      readBuffer[2],
                      readBuffer[3]);

            return EN_ERROR_FAILED_TO_WAKE_ATMEL_ATSHA204A;
        }
    }

#ifdef _DEBUG
    EN_PRINTF("Atmel ATSHA204A device woken.\r\n");
#endif

    return EN_SUCCESS;
}

EN_RESULT AtmelAtsha204a_Sleep()
{
    uint8_t sleepCommand = 0x01;
    I2cWrite(
        ATMEL_ATSHA204A_DEVICE_ADDRESS, 0, EI2cSubAddressMode_OneByte, (uint8_t*)&sleepCommand, sizeof(sleepCommand));

    return EN_SUCCESS;
}


/**
 * \brief Get the length of a command packet, which is dependent on how much additional data
 * the command requires in addition to the standard count, parameters and CRC.
 *
 * @param[in] additionalDataLengthBytes		The amount of additional data
 * @return									EN_RESULT code
 */
uint8_t AtmelAtsha204a_GetCommandPacketSize(uint8_t additionalDataLengthBytes)
{
    // Input block length is count + opcode + param 1 + param 2 + data + checksum
    uint8_t packetSizeBytes = COMMAND_PACKET_COUNT_SIZE_BYTES + COMMAND_PACKET_OPCODE_LENGTH_BYTES +
                              COMMAND_PACKET_PARAM1_SIZE_BYTES + COMMAND_PACKET_PARAM2_SIZE_BYTES +
                              additionalDataLengthBytes + CHECKSUM_LENGTH_BYTES;

    return packetSizeBytes;
}


/**
 * \brief Encode an address for reading or writing.
 *
 * @param[in] zone				Zone select
 * @param[in] slotIndex			Slot select
 * @param[in] wordOffset		Word offset within the given slot
 * @param[out] encodedAddress	The encoded address
 * @return
 */
EN_RESULT AtmelAtsha204a_EncodeAddress(EZoneSelect_t zone,
                                       uint8_t slotIndex,
                                       uint8_t wordOffset,
                                       uint16_t* pEncodedAddress)
{
    if (pEncodedAddress == NULL)
    {
        return EN_ERROR_NULL_POINTER;
    }

    *pEncodedAddress = 0;
    uint16_t rawEncodedAddress = 0;

    if (wordOffset > MAX_WORD_OFFSET)
    {
        return EN_ERROR_ATSHA204A_INVALID_ADDRESS_PARAMETER;
    }

    switch (zone)
    {
    case EZoneSelect_Config:
    {
        if (slotIndex > (CONFIGURATION_ZONE_SIZE_SLOTS - 1))
        {
            return EN_ERROR_ATSHA204A_INVALID_ADDRESS_PARAMETER;
        }

        if (slotIndex == 2 && wordOffset > CONFIGURATION_ZONE_SLOT_2_MAX_WORD_OFFSET)
        {
            return EN_ERROR_ATSHA204A_INVALID_ADDRESS_PARAMETER;
        }

        break;
    }
    case EZoneSelect_Otp:
    {
        if (slotIndex > (OTP_ZONE_SIZE_SLOTS - 1))
        {
            return EN_ERROR_ATSHA204A_INVALID_ADDRESS_PARAMETER;
        }

        break;
    }
    case EZoneSelect_Data:
    {
        if (slotIndex > (DATA_ZONE_SIZE_SLOTS - 1))
        {
            return EN_ERROR_ATSHA204A_INVALID_ADDRESS_PARAMETER;
        }

        break;
    }
    default:
        break;
    }


    rawEncodedAddress |= slotIndex << 3;
    rawEncodedAddress |= wordOffset;

    // Address data is transmitted with the lower byte first - swap the bytes here so that they're presented correctly
    // to the ConstructCommandPacket function.
    *pEncodedAddress |= GetLowerByte(rawEncodedAddress) << 8;
    *pEncodedAddress |= GetUpperByte(rawEncodedAddress);

    return EN_SUCCESS;
}


/**
 *
 * \brief Construct a command packet.
 *
 * @param[in] command						The command
 * @param[in] parameter1					Parameter 1
 * @param[in] parameter2					Parameter 2
 * @param[in] additionalDataLengthBytes		Length of additional data
 * @param[in] pAdditionalData				Additional data buffer, may be NULL
 * @param[out] pCommandPacket				The constructed command packet
 * @return									EN_RESULT code
 */
EN_RESULT AtmelAtsha204a_ConstructCommandPacket(ECommand_t command,
                                                uint8_t parameter1,
                                                uint16_t parameter2,
                                                uint8_t additionalDataLengthBytes,
                                                uint8_t* pAdditionalData,
                                                uint8_t* pCommandPacket)
{
    if (pCommandPacket == NULL)
    {
        return EN_ERROR_NULL_POINTER;
    }

    uint8_t commandPacketLength = AtmelAtsha204a_GetCommandPacketSize(additionalDataLengthBytes);
    *(pCommandPacket + COMMAND_PACKET_COUNT_BYTE_INDEX) = commandPacketLength;

    *(pCommandPacket + COMMAND_PACKET_OPCODE_BYTE_INDEX) = (uint8_t)command;
    *(pCommandPacket + COMMAND_PACKET_PARAM1_BYTE_INDEX) = parameter1;
    *(pCommandPacket + COMMAND_PACKET_PARAM2_BYTE_INDEX) = GetUpperByte(parameter2);
    *(pCommandPacket + COMMAND_PACKET_PARAM2_BYTE_INDEX + 1) = GetLowerByte(parameter2);

    if (additionalDataLengthBytes > 0)
    {
        unsigned int dataStartByteIndex = COMMAND_PACKET_COUNT_SIZE_BYTES + COMMAND_PACKET_OPCODE_LENGTH_BYTES +
                                          COMMAND_PACKET_PARAM1_SIZE_BYTES + COMMAND_PACKET_PARAM2_SIZE_BYTES;

        unsigned int byteIndex = 0;
        for (byteIndex = 0; byteIndex < additionalDataLengthBytes; byteIndex++)
        {
            *(pCommandPacket + dataStartByteIndex + byteIndex) = pAdditionalData[byteIndex];
        }
    }


    uint16_t checksum = AtmelAtsha204a_CalculateCrc(pCommandPacket, commandPacketLength - CHECKSUM_LENGTH_BYTES);

    *(pCommandPacket + commandPacketLength - 2) = GetLowerByte(checksum);
    *(pCommandPacket + commandPacketLength - 1) = GetUpperByte(checksum);

    return EN_SUCCESS;
};


/**
 * \brief Check that a response from the device is valid by verifying the CRC.
 *
 * @param[in] pResponse 	Pointer to response data
 * @return					EN_RESULT code
 */
EN_RESULT AtmelAtsha20a4_CheckResponseCrc(const uint8_t* pResponse)
{
    if (pResponse == NULL)
    {
        return EN_ERROR_NULL_POINTER;
    }

    uint8_t responseSize = pResponse[STATUS_RESPONSE_COUNT_BYTE_INDEX];
    uint8_t crcPosition = responseSize - CHECKSUM_LENGTH_BYTES;
    uint16_t receivedCrc = pResponse[crcPosition] | pResponse[crcPosition + 1] << 8;

    uint16_t expectedCrc = AtmelAtsha204a_CalculateCrc(pResponse, responseSize - CHECKSUM_LENGTH_BYTES);

    if (receivedCrc != expectedCrc)
    {
        EN_PRINTF("Atmel ATSHA204 CRC error: expected 0x%x, received 0x%x\r\n", expectedCrc, receivedCrc);

        return EN_ERROR_ATSHA204A_INVALID_RESPONSE_CRC;
    }

    return EN_SUCCESS;
}


/**
 * \brief Parse a response from the device, checking its CRC and then determining the error code if an
 * error has occurred.
 *
 * @param[in] pResponseBlock	Pointer to response data
 * @return						EN_RESULT code
 */
EN_RESULT AtmelAtsha20a4_CheckCommandResponseBlock(const uint8_t* pResponseBlock)
{
    if (pResponseBlock == NULL)
    {
        return EN_ERROR_NULL_POINTER;
    }

    // Extract data from the response.
    EN_RETURN_IF_FAILED(AtmelAtsha20a4_CheckResponseCrc(pResponseBlock));

    EStatusCode_t status = (EStatusCode_t) * (pResponseBlock + STATUS_RESPONSE_STATUS_BYTE_INDEX);

    if (status != EStatusCode_Success)
    {
        switch (status)
        {

        case EStatusCode_InvalidMac:
        {
            EN_PRINTF("Atmel ATSHA204A error - invalid MAC\r\n");
            return EN_ERROR_ATSHA204A_INVALID_MAC;
        }
        case EStatusCode_ParseError:
        {
            EN_PRINTF("Atmel ATSHA204A error - parse error\r\n");
            return EN_ERROR_ATSHA204A_PARSE_ERROR;
        }
        case EStatusCode_ExecutionError:
        {
            EN_PRINTF("Atmel ATSHA204A error - execution error\r\n");
            return EN_ERROR_ATSHA204A_EXECUTION_ERROR;
        }
        case EStatusCode_AfterWake:
        {
            EN_PRINTF("Atmel ATSHA204A error - first command after wake\r\n");
            return EN_ERROR_ATSHA204A_FIRST_COMMAND_AFTER_WAKE;
        }
        case EStatusCode_IoError:
        {
            EN_PRINTF("Atmel ATSHA204A error - CRC or other IO error\r\n");
            return EN_ERROR_ATSHA204A_IO_ERROR;
        }
        default:
        {
            return EN_ERROR_ATSHA204A_IO_ERROR;
        }
        }
    }

    return EN_SUCCESS;
}


/**
 * \brief Perform a read from the device's output buffer in response to a sent Read command.
 *
 * @param[in] numberOfBytesToRead	The number of bytes to read
 * @param[out] pReadData			Pointer to buffer to receive read data
 * @return							EN_RESULT code
 */
EN_RESULT AtmelAtsha204a_ReadDataResponse(uint8_t numberOfBytesToRead, uint8_t* pReadData)
{
    if (pReadData == NULL)
    {
        return EN_ERROR_NULL_POINTER;
    }

    // Response packets also contain a count byte and a 2-byte checksum.
    uint8_t totalResponsePacketSizeBytes = numberOfBytesToRead + 1 + CHECKSUM_LENGTH_BYTES;
    uint8_t completeResponsePacket[7];

    EN_RESULT result;
    do
    {
        result = I2cRead(ATMEL_ATSHA204A_DEVICE_ADDRESS,
                         0,
                         EI2cSubAddressMode_None,
                         totalResponsePacketSizeBytes,
                         (uint8_t*)&completeResponsePacket);
    } while (EN_FAILED(result));


    uint8_t responseSize = completeResponsePacket[STATUS_RESPONSE_COUNT_BYTE_INDEX];

    if (responseSize != totalResponsePacketSizeBytes && responseSize == STATUS_RESPONSE_BLOCK_SIZE_BYTES)
    {
        return AtmelAtsha20a4_CheckCommandResponseBlock(completeResponsePacket);
    }

    EN_RETURN_IF_FAILED(AtmelAtsha20a4_CheckResponseCrc((uint8_t*)&completeResponsePacket));

    // Copy the response data to the output.
    unsigned int dataByteIndex = 0;
    for (dataByteIndex = 0; dataByteIndex < numberOfBytesToRead; dataByteIndex++)
    {
        pReadData[dataByteIndex] = completeResponsePacket[dataByteIndex + 1];
    }

    return EN_SUCCESS;
}


/**
 * \brief Send a command to the device.
 *
 * @param[in] pCommandPacket			Command packet
 * @param[in] commandPacketLengthBytes		Length of the command packet
 * @return								EN_RESULT code
 */
EN_RESULT AtmelAtsha204a_SendCommand(const uint8_t* pCommandPacket, uint8_t commandPacketLengthBytes)
{
    if (pCommandPacket == NULL)
    {
        return EN_ERROR_NULL_POINTER;
    }

    AtmelAtsha204a_Wake(true);

    EN_RETURN_IF_FAILED(I2cWrite(ATMEL_ATSHA204A_DEVICE_ADDRESS,
                                 EPacketFunction_Command,
                                 EI2cSubAddressMode_OneByte,
                                 pCommandPacket,
                                 commandPacketLengthBytes));

    return EN_SUCCESS;
}


EN_RESULT AtmelAtsha204a_Read(EReadSizeSelect_t sizeSelect,
                              EZoneSelect_t zoneSelect,
                              uint16_t encodedAddress,
                              uint8_t* pReadData)
{
    if (pReadData == NULL)
    {
        return EN_ERROR_NULL_POINTER;
    }

    uint8_t zone = (uint8_t)zoneSelect;
    uint8_t numberOfBytesToRead = 0;

    switch (sizeSelect)
    {
    case EReadSizeSelect_4Bytes:
    {
        numberOfBytesToRead = 4;
        break;
    }
    case EReadSizeSelect_32Bytes:
    {
        numberOfBytesToRead = 32;

        // Set bit 7 to indicate a 32-byte write. The exception is if we're reading from the OTP zone, for which bit 7
        // must always be 0.
        if (zoneSelect != EZoneSelect_Otp)
        {
            zone |= 1 << 7;
        }

        break;
    }
    default:
        break;
    }

#ifdef _DEBUG
    EN_PRINTF("Atmel ATSHA204A: reading %d bytes from zone %d, encoded address %d\r\n",
              sizeSelect,
              zoneSelect,
              encodedAddress);
#endif

    uint8_t commandPacket[7];
    AtmelAtsha204a_ConstructCommandPacket(ECommand_Read, zone, encodedAddress, 0, NULL, (uint8_t*)&commandPacket);

    EN_RETURN_IF_FAILED(AtmelAtsha204a_SendCommand((uint8_t*)&commandPacket, 7));

#ifndef _DEBUG
    // Wait for the read command to  be processed.
    SleepMilliseconds(ATMEL_ATSHA204A_READ_EXECUTION_TIME_MILLISECONDS);
#endif

    // Read the response.
    EN_RETURN_IF_FAILED(AtmelAtsha204a_ReadDataResponse(numberOfBytesToRead, pReadData));

    AtmelAtsha204a_Sleep();

    return EN_SUCCESS;
}
