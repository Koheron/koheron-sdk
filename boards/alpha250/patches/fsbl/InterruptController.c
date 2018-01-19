/** \file InterruptController.c
 * \brief Implementation file for interrupt controller functions.
 * \author Garry Jeromson
 * \date 24.06.2015
 */

//-------------------------------------------------------------------------------------------------
// Includes
//-------------------------------------------------------------------------------------------------

#include "InterruptController.h"
#include "I2cInterfaceVariables.h"
#include "TimerInterfaceVariables.h"

#include <xil_exception.h>
#include <xscugic.h>
#include <xparameters.h>

//-------------------------------------------------------------------------------------------------
// Definitions and constants
//-------------------------------------------------------------------------------------------------

#define INTC_DEVICE_ID XPAR_SCUGIC_SINGLE_DEVICE_ID

//-------------------------------------------------------------------------------------------------
// Global variable definitions
//-------------------------------------------------------------------------------------------------

/// Interrupt controller, declared in InterruptController.h
XScuGic g_interruptController;

//-------------------------------------------------------------------------------------------------
// Function definitions
//-------------------------------------------------------------------------------------------------

EN_RESULT SetupInterruptSystem()
{

    XScuGic_Config* pInterruptControllerConfig;

    // Initialize the exception table.
    Xil_ExceptionInit();

    // Initialize the interrupt controller driver so that it's ready to use.
    pInterruptControllerConfig = XScuGic_LookupConfig(INTC_DEVICE_ID);

    if (NULL == pInterruptControllerConfig)
    {
        return EN_ERROR_NULL_POINTER;
    }

    RETURN_IF_XILINX_CALL_FAILED(XScuGic_CfgInitialize(&g_interruptController,
                                                       pInterruptControllerConfig,
                                                       pInterruptControllerConfig->CpuBaseAddress),
                                 EN_ERROR_FAILED_TO_INITIALISE_INTERRUPT_CONTROLLER);

    // Register the interrupt controller handler with the exception table.
    Xil_ExceptionRegisterHandler(
        XIL_EXCEPTION_ID_IRQ_INT, (Xil_ExceptionHandler)XScuGic_InterruptHandler, &g_interruptController);

    // Connect the device driver handler that will be called when an I2C interrupt
    // occurs
    RETURN_IF_XILINX_CALL_FAILED(XScuGic_Connect(&g_interruptController,
                                                 IIC_INTR_ID,
                                                 (Xil_InterruptHandler)XIicPs_MasterInterruptHandler,
                                                 &g_XIicPsInstance),
                                 EN_ERROR_FAILED_TO_INITIALISE_INTERRUPT_CONTROLLER);

    // Enable the interrupts for the IIC device.
    XScuGic_Enable(&g_interruptController, IIC_INTR_ID);

    // INSERT ANY FURTHER INTERRUPT ENABLES HERE //

    // Enable non-critical exceptions.
    Xil_ExceptionEnable();

    return EN_SUCCESS;
}
