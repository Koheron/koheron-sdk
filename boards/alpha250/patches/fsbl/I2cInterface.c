/** \file I2cInterface.c
 * \brief Implementation file for I2C interface functions.
 * \author Garry Jeromson
 * \date 18.06.15
 */

//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "I2cInterface.h"
#include "I2cInterfaceVariables.h"
#include "SystemDefinitions.h"
#include "UtilityFunctions.h"
#include "TimerInterface.h"
#include "InterruptController.h"
#include "ErrorCodes.h"

//-------------------------------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------------------------------

const unsigned int I2C_CLOCK_SPEED_HZ = 100000;


//-------------------------------------------------------------------------------------------------
// Global variable definitions
//-------------------------------------------------------------------------------------------------

XIicPs g_XIicPsInstance;
XIicPs_Config* g_pXIicPsConfig;

volatile bool g_i2cTransmissionInProgress;
volatile bool g_i2cReceiveInProgress;
volatile bool g_i2cSlaveNack;

volatile uint32_t g_transmissionErrorCount;

//-------------------------------------------------------------------------------------------------
// Function definitions
//-------------------------------------------------------------------------------------------------


/**
 * This Status handler is called asynchronously from an interrupt
 * context and indicates the events that have occurred.
 *
 * @param	InstancePtr is a pointer to the IIC driver instance for which
 *		the handler is being called for.
 * @param	Event indicates the condition that has occurred.
 *
 * @return	None.
 *
 * @note		None.
 *
 */
void StatusHandler(void* InstancePtr, int event)
{
    if (event & XIICPS_EVENT_COMPLETE_RECV)
    {
        g_i2cReceiveInProgress = false;

#ifdef _DEBUG
        EN_PRINTF("Event = receive complete\r\n");
#endif
    }

    if (event & XIICPS_EVENT_COMPLETE_SEND)
    {
        g_i2cTransmissionInProgress = false;

#ifdef _DEBUG
        EN_PRINTF("Event = send complete\r\n");
#endif
    }


    if ((event & XIICPS_EVENT_SLAVE_RDY) == 0)
    {

        g_transmissionErrorCount++;

#ifdef _DEBUG
        EN_PRINTF("Data received with error\n\r");
#endif
    }

    if (event & XIICPS_EVENT_NACK)
    {
        g_i2cSlaveNack = true;

#ifdef _DEBUG
        EN_PRINTF("Event = NACK received\r\n");
#endif
    }

#ifdef _DEBUG
    if (event & XIICPS_EVENT_TIME_OUT)
    {
        EN_PRINTF("Event = timeout\r\n");
    }

    if (event & XIICPS_EVENT_ERROR)
    {
        EN_PRINTF("Event = error\r\n");
    }

    if (event & XIICPS_EVENT_ARB_LOST)
    {
        EN_PRINTF("Event = arbitration lost\r\n");
    }

    if (event & XIICPS_EVENT_SLAVE_RDY)
    {
        EN_PRINTF("Event = slave ready\r\n");
    }

    if (event & XIICPS_EVENT_RX_OVR)
    {
        EN_PRINTF("Event = receive overflow\r\n");
    }

    if (event & XIICPS_EVENT_TX_OVR)
    {
        EN_PRINTF("Event = transmit overflow\r\n");
    }

    if (event & XIICPS_EVENT_RX_UNF)
    {
        EN_PRINTF("Event = receive underflow\r\n");
    }

#endif
}

EN_RESULT InitialiseI2cInterface()
{

    g_pXIicPsConfig = XIicPs_LookupConfig(IIC_DEVICE_ID);
    if (g_pXIicPsConfig == NULL)
    {
        return EN_ERROR_FAILED_TO_INITIALISE_I2C_CONTROLLER;
    }

    RETURN_IF_XILINX_CALL_FAILED(XIicPs_CfgInitialize(&g_XIicPsInstance, g_pXIicPsConfig, g_pXIicPsConfig->BaseAddress),
                                 EN_ERROR_FAILED_TO_INITIALISE_I2C_CONTROLLER);


    RETURN_IF_XILINX_CALL_FAILED(XIicPs_SelfTest(&g_XIicPsInstance), EN_ERROR_FAILED_TO_INITIALISE_I2C_CONTROLLER);

    EN_RETURN_IF_FAILED(SetupInterruptSystem());

    // Set the status handler.
    XIicPs_SetStatusHandler(&g_XIicPsInstance, (void*)&g_XIicPsInstance, (XIicPs_IntrHandler)StatusHandler);

    // Set I2C clock to 100kHz
    XIicPs_SetSClk(&g_XIicPsInstance, I2C_CLOCK_SPEED_HZ);

    return EN_SUCCESS;
}


EN_RESULT I2cAbort()
{
    XIicPs_Abort(&g_XIicPsInstance);
    return EN_SUCCESS;
}

EN_RESULT I2cWrite_NoSubAddress(uint8_t deviceAddress, const uint8_t* pWriteBuffer, uint32_t numberOfBytesToWrite)
{
    if (pWriteBuffer == NULL)
    {
        return EN_ERROR_NULL_POINTER;
    }

    if (numberOfBytesToWrite == 0)
    {
        return EN_ERROR_INVALID_ARGUMENT;
    }

#ifdef _DEBUG
    xil_printf("I2C: Writing %d bytes to device address 0x%x\r\n", numberOfBytesToWrite, deviceAddress);
#endif

    // Wait for bus to become idle
    while (XIicPs_BusIsBusy(&g_XIicPsInstance))
    {
        /* NOP */
    }

    // Set the transmission flags.
    g_transmissionErrorCount = 0;
    g_i2cTransmissionInProgress = true;
    g_i2cSlaveNack = false;

    XIicPs_MasterSend(&g_XIicPsInstance, (uint8_t*)pWriteBuffer, numberOfBytesToWrite, deviceAddress);

    // Wait till data is transmitted.
    unsigned int timeout = 0;
    while ((g_i2cTransmissionInProgress && !g_i2cSlaveNack) || (XIicPs_BusIsBusy(&g_XIicPsInstance)))

    {
        SleepMilliseconds(1);
        timeout++;
        if (timeout > 100)
        {
#ifdef _DEBUG
            xil_printf(
                "Error: I2C timeout when writing %d bytes to device 0x%x\r\n", numberOfBytesToWrite, deviceAddress);
#endif

            return EN_ERROR_I2C_WRITE_TIMEOUT;
        }
    }

    if (g_i2cSlaveNack)
    {
#ifdef _DEBUG
        xil_printf("NACK received from I2C slave at address 0x%x\n\r", deviceAddress);
#endif

        return EN_ERROR_I2C_SLAVE_NACK;
    }

    return EN_SUCCESS;
}

EN_RESULT I2cWrite_ByteSubAddress(uint8_t deviceAddress,
                                  uint8_t subAddress,
                                  const uint8_t* pWriteBuffer,
                                  uint32_t numberOfBytesToWrite)
{
    // Create a new array, to contain both the subaddress and the write data.
    uint8_t transferSizeBytes = numberOfBytesToWrite + 1;
    uint8_t transferData[transferSizeBytes];
    transferData[0] = subAddress;

    unsigned int dataByteIndex = 0;
    for (dataByteIndex = 0; dataByteIndex < numberOfBytesToWrite; dataByteIndex++)
    {
        transferData[dataByteIndex + 1] = pWriteBuffer[dataByteIndex];
    }

    EN_RETURN_IF_FAILED(I2cWrite_NoSubAddress(deviceAddress, (uint8_t*)&transferData, transferSizeBytes));

    return EN_SUCCESS;
}

EN_RESULT I2cWrite_TwoByteSubAddress(uint8_t deviceAddress,
                                     uint16_t subAddress,
                                     const uint8_t* pWriteBuffer,
                                     uint32_t numberOfBytesToWrite)
{
    // Create a new array, to contain both the subaddress and the write data.
    uint8_t transferSizeBytes = numberOfBytesToWrite + 2;
    uint8_t transferData[transferSizeBytes];
    transferData[0] = GetUpperByte(subAddress);
    transferData[1] = GetLowerByte(subAddress);

    unsigned int dataByteIndex = 0;
    for (dataByteIndex = 0; dataByteIndex < numberOfBytesToWrite; dataByteIndex++)
    {
        transferData[dataByteIndex + 2] = pWriteBuffer[dataByteIndex];
    }

    EN_RETURN_IF_FAILED(I2cWrite_NoSubAddress(deviceAddress, (uint8_t*)&transferData, transferSizeBytes));

    return EN_SUCCESS;
}

EN_RESULT I2cRead_NoSubAddress(uint8_t deviceAddress, uint8_t* pReadBuffer, uint32_t numberOfBytesToRead)
{

    if (pReadBuffer == NULL)
    {
        return EN_ERROR_NULL_POINTER;
    }

    if (numberOfBytesToRead == 0)
    {
        return EN_ERROR_INVALID_ARGUMENT;
    }

#ifdef _DEBUG
    xil_printf("I2C: Reading %d bytes from device address 0x%x\r\n", numberOfBytesToRead, deviceAddress);
#endif

    // Wait for bus to become idle
    while (XIicPs_BusIsBusy(&g_XIicPsInstance))
    {
        /* NOP */
    }

    // Set the transmission flags.
    g_transmissionErrorCount = 0;
    g_i2cReceiveInProgress = true;
    g_i2cSlaveNack = false;

    // Receive the data.
    XIicPs_MasterRecv(&g_XIicPsInstance, pReadBuffer, numberOfBytesToRead, deviceAddress);

    // Wait till all the data is received.
    unsigned int timeout = 0;
    while (g_i2cReceiveInProgress && !g_i2cSlaveNack)
    {
        SleepMilliseconds(1);
        timeout++;
        if (timeout > 1000)
        {
#ifdef _DEBUG
            xil_printf(
                "Error: I2C timeout when receiving %d bytes from device 0x%x\r\n", numberOfBytesToRead, deviceAddress);
#endif
            return EN_ERROR_I2C_READ_TIMEOUT;
        }
    }

    if (g_i2cSlaveNack)
    {
#ifdef _DEBUG
        xil_printf("NACK received from I2C slave at address 0x%x\n\r", deviceAddress);
#endif

        return EN_ERROR_I2C_SLAVE_NACK;
    }

    return EN_SUCCESS;
}

EN_RESULT I2cRead_ByteSubAddress(uint8_t deviceAddress,
                                 uint8_t subAddress,
                                 uint8_t* pReadBuffer,
                                 uint32_t numberOfBytesToRead)
{
    // Write the subaddress, with start condition asserted but stop condition not.
    EN_RETURN_IF_FAILED(I2cWrite_NoSubAddress(deviceAddress, (uint8_t*)&subAddress, 1));

    // Perform the read.
    EN_RETURN_IF_FAILED(I2cRead_NoSubAddress(deviceAddress, pReadBuffer, numberOfBytesToRead));

    return EN_SUCCESS;
}

EN_RESULT I2cRead_WordSubAddress(uint8_t deviceAddress,
                                 uint16_t subAddress,
                                 uint8_t* pReadBuffer,
                                 uint32_t numberOfBytesToRead)
{
    // Write the subaddress, with start condition asserted but stop condition not.
    EN_RETURN_IF_FAILED(I2cWrite_NoSubAddress(deviceAddress, (uint8_t*)&subAddress, 2));

    // Perform the read.
    EN_RETURN_IF_FAILED(I2cRead_NoSubAddress(deviceAddress, pReadBuffer, numberOfBytesToRead));

    return EN_SUCCESS;
}

EN_RESULT I2cRead(uint8_t deviceAddress,
                  uint16_t subAddress,
                  EI2cSubAddressMode_t subAddressMode,
                  uint32_t numberOfBytesToRead,
                  uint8_t* pReadBuffer)
{

    switch (subAddressMode)
    {
    case EI2cSubAddressMode_None:
    {
        EN_RETURN_IF_FAILED(I2cRead_NoSubAddress(deviceAddress, pReadBuffer, numberOfBytesToRead));
        break;
    }
    case EI2cSubAddressMode_OneByte:
    {
        EN_RETURN_IF_FAILED(
            I2cRead_ByteSubAddress(deviceAddress, (uint8_t)subAddress, pReadBuffer, numberOfBytesToRead));
        break;
    }
    case EI2cSubAddressMode_TwoBytes:
    {
        EN_RETURN_IF_FAILED(
            I2cRead_WordSubAddress(deviceAddress, (uint16_t)subAddress, pReadBuffer, numberOfBytesToRead));
    }
    default:
        break;
    }

    return EN_SUCCESS;
}

EN_RESULT I2cWrite(uint8_t deviceAddress,
                   uint16_t subAddress,
                   EI2cSubAddressMode_t subAddressMode,
                   const uint8_t* pWriteBuffer,
                   uint32_t numberOfBytesToWrite)
{

    switch (subAddressMode)
    {
    case EI2cSubAddressMode_None:
    {
        EN_RETURN_IF_FAILED(I2cWrite_NoSubAddress(deviceAddress, pWriteBuffer, numberOfBytesToWrite));
        break;
    }
    case EI2cSubAddressMode_OneByte:
    {
        EN_RETURN_IF_FAILED(
            I2cWrite_ByteSubAddress(deviceAddress, (uint8_t)subAddress, pWriteBuffer, numberOfBytesToWrite));
        break;
    }
    case EI2cSubAddressMode_TwoBytes:
    {
        EN_RETURN_IF_FAILED(I2cWrite_TwoByteSubAddress(deviceAddress, subAddress, pWriteBuffer, numberOfBytesToWrite));
        break;
    }
    default:
        break;
    }

    return EN_SUCCESS;
}
