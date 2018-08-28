/******************************************************************************
*
* Copyright (C) 2012 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* XILINX CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*****************************************************************************
*
* @file fsbl_hooks.c
*
* This file provides functions that serve as user hooks.  The user can add the
* additional functionality required into these routines.  This would help retain
* the normal FSBL flow unchanged.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date        Changes
* ----- ---- -------- -------------------------------------------------------
* 3.00a np   08/03/12 Initial release
* </pre>
*
* @note
*
******************************************************************************/

#include <mini-printf.h>

#include "fsbl.h"
#include "xstatus.h"
#include "fsbl_hooks.h"
#include "xemacps.h"


//-------------------------------------------------------------------------------------------------
// Enclustra Includes
//-------------------------------------------------------------------------------------------------

#include "I2cInterface.h"
#include "InterruptController.h"
#include "ModuleEeprom.h"

/************************** Variable Definitions *****************************/


/************************** Function Prototypes ******************************/

static int GetMacAddress(void);


/******************************************************************************
* This function is the hook which will be called  before the bitstream download.
* The user can add all the customized code required to be executed before the
* bitstream download to this routine.
*
* @param None
*
* @return
*		- XST_SUCCESS to indicate success
*		- XST_FAILURE.to indicate failure
*
****************************************************************************/
u32 FsblHookBeforeBitstreamDload(void)
{
	u32 Status;

	Status = XST_SUCCESS;

	/*
	 * User logic to be added here. Errors to be stored in the status variable
	 * and returned
	 */
	fsbl_printf(DEBUG_INFO,"In FsblHookBeforeBitstreamDload function \r\n");

	return (Status);
}

/******************************************************************************
* This function is the hook which will be called  after the bitstream download.
* The user can add all the customized code required to be executed after the
* bitstream download to this routine.
*
* @param None
*
* @return
*		- XST_SUCCESS to indicate success
*		- XST_FAILURE.to indicate failure
*
****************************************************************************/
u32 FsblHookAfterBitstreamDload(void)
{
	u32 Status;

	Status = XST_SUCCESS;

	/*
	 * User logic to be added here.
	 * Errors to be stored in the status variable and returned
	 */
	fsbl_printf(DEBUG_INFO, "In FsblHookAfterBitstreamDload function \r\n");

	return (Status);
}

/******************************************************************************
* This function is the hook which will be called  before the FSBL does a handoff
* to the application. The user can add all the customized code required to be
* executed before the handoff to this routine.
*
* @param None
*
* @return
*		- XST_SUCCESS to indicate success
*		- XST_FAILURE.to indicate failure
*
****************************************************************************/
u32 FsblHookBeforeHandoff(void)
{
	u32 Status;

	Status = XST_SUCCESS;

	/*
	 * User logic to be added here.
	 * Errors to be stored in the status variable and returned
	 */

	Status = GetMacAddress();
	if (Status != XST_SUCCESS) {
		fsbl_printf(DEBUG_GENERAL,"Error getting MAC Address\r\n");
	} else {
		fsbl_printf(DEBUG_GENERAL,"MAC address retrieved\r\n");
	}

	return XST_SUCCESS;
}


/******************************************************************************
* This function is the hook which will be called in case FSBL fall back
*
* @param None
*
* @return None
*
****************************************************************************/
void FsblHookFallback(void)
{
	/*
	 * User logic to be added here.
	 * Errors to be stored in the status variable and returned
	 */
	fsbl_printf(DEBUG_INFO,"In FsblHookFallback function \r\n");
	while(1);
}


/******************************************************************************
* This function performs required initialisation of the system resources used by
* the application
*
* @param None
*
* @return None
*
****************************************************************************/

EN_RESULT InitaliseSystem()
{
    EN_RETURN_IF_FAILED(InitialiseI2cInterface());

    EN_RETURN_IF_FAILED(SetupInterruptSystem());

    return EN_SUCCESS;
}

/******************************************************************************/
/**
*
* This function configures the Ethernet PHY
* The RGMII delays are enabled on the clock lines for propprt timing
*
* @param None
*
* @return
* return  status
*
****************************************************************************/

static int GetMacAddress(void)
{

	if (EN_FAILED(InitaliseSystem()))
    {
        return -1;
    }

	XEmacPs EmacPsInstance;
	XEmacPs *EmacPsInstancePtr = (XEmacPs*) &EmacPsInstance;
	int Status;
	u16 PhyData, PhyAddr, PhyType;
	XEmacPs_Config *Config;
	volatile int i;

    uint32_t serialNumber;
    ProductNumberInfo_t productNumberInfo;
    uint8_t macAddress[6];
    char EmacPsMAC[6];
    int MacAddrSet = 0;

    // Initialise the EEPROM.
	if (EN_SUCCEEDED(Eeprom_Initialise()))
    {
		// Read the EEPROM.
		if (EN_SUCCEEDED(Eeprom_Read()))
		{
		    // After reading the EEPROM, the information is stored in its own translation unit - we can
		    // query it using the EEPROM API functions.
			if (EN_SUCCEEDED(Eeprom_GetModuleInfo(&serialNumber, &productNumberInfo, (uint64_t*)&macAddress))){
				fsbl_printf(DEBUG_GENERAL,"MAC address configured successfully from EEPROM\n\r");
				EmacPsMAC[0] = macAddress[5];
				EmacPsMAC[1] = macAddress[4];
				EmacPsMAC[2] = macAddress[3];
				EmacPsMAC[3] = macAddress[2];
				EmacPsMAC[4] = macAddress[1];
				EmacPsMAC[5] = macAddress[0];
				MacAddrSet = 1;
			}

		}
    }
	if (MacAddrSet == 0){
		EN_PRINTF("Error reading EEPROM, using default MAC address\r\n");
		EmacPsMAC[0] = 0x00;
		EmacPsMAC[1] = 0x0a;
		EmacPsMAC[2] = 0x35;
		EmacPsMAC[3] = 0x01;
		EmacPsMAC[4] = 0x02;
		EmacPsMAC[5] = 0x03;
	}


    // https://forums.xilinx.com/t5/Embedded-Linux/MAC-address-EEPROM-Petalinux/td-p/668780
    // Saving MAC address in the OCM memory at 0xFFFF_FC00
	// Create string to copy in the OCM

	char ethaddr[40];
    mini_snprintf(ethaddr, sizeof ethaddr, "ethaddr=%02x:%02x:%02x:%02x:%02x:%02x\n", EmacPsMAC[0], EmacPsMAC[1], EmacPsMAC[2], EmacPsMAC[3], EmacPsMAC[4], EmacPsMAC[5]);
	// Copy string to memory
	for(i=0; i< strlen(ethaddr)+1; i++)
	    Xil_Out8(0xFFFFFC00+i, ethaddr[i]);

}



