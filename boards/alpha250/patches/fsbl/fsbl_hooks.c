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
#include "TimerInterface.h"

/************************** Variable Definitions *****************************/


/************************** Function Prototypes ******************************/

static int ConfigEthPhy(void);


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

	Status = ConfigEthPhy();
	if (Status != XST_SUCCESS) {
		fsbl_printf(DEBUG_GENERAL,"Error configuring PHY\r\n");
	} else {
		fsbl_printf(DEBUG_GENERAL,"PHY configured\r\n");
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
    EN_RETURN_IF_FAILED(InitialiseTimer());

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

static int ConfigEthPhy(void)
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


	Config = XEmacPs_LookupConfig(XPAR_XEMACPS_0_DEVICE_ID);

	Status = XEmacPs_CfgInitialize(EmacPsInstancePtr, Config,
					Config->BaseAddress);

	if (Status != XST_SUCCESS) {
		fsbl_printf(DEBUG_INFO,"Error in initialize\r\n");
		return XST_FAILURE;
	}

	/*
	 * Set the MAC address
	 */
	Status = XEmacPs_SetMacAddress(EmacPsInstancePtr, EmacPsMAC, 1);
	if (Status != XST_SUCCESS) {
		fsbl_printf(DEBUG_INFO,"Error setting MAC address\r\n");
		return XST_FAILURE;
	}

	XEmacPs_SetMdioDivisor(EmacPsInstancePtr, MDC_DIV_224);

	// detect PHY
	PhyAddr = 3;
	XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr, 0x3, (u16*)&PhyData);  // read value
	PhyType = (PhyData >> 4);

	// enabling RGMII delays
	if (PhyType == 0x162){ // KSZ9031
		fsbl_printf(DEBUG_GENERAL,"Detected KSZ9031 Ethernet PHY\n\r");
		//Ctrl Delay
		u16 RxCtrlDelay=7; // 0..15, default 7
		u16 TxCtrlDelay=7; // 0..15, default 7
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xD, 0x0002);
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xE, 0x0004); // Reg 0x4
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xD, 0x4002);
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xE, (TxCtrlDelay+(RxCtrlDelay<<4)));
		//Data Delay
		u16 RxDataDelay=7; // 0..15, default 7
		u16 TxDataDelay=7; // 0..15, default 7
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xD, 0x0002);
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xE, 0x0005); // Reg 0x5
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xD, 0x4002);
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xE, (RxDataDelay+(RxDataDelay << 4)+(RxDataDelay << 8)+(RxDataDelay << 12)));
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xD, 0x0002);
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xE, 0x0006); // Reg 0x6
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xD, 0x4002);
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xE, (TxDataDelay+(TxDataDelay << 4)+(TxDataDelay << 8)+(TxDataDelay << 12)));
		//Clock Delay
		u16 RxClockDelay=31; // 0..31, default 15
		u16 TxClockDelay=31; // 0..31, default 15
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xD, 0x0002);
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xE, 0x0008); // Reg 0x8 RGMII Clock Pad Skew
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xD, 0x4002);
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xE, (RxClockDelay+(TxClockDelay<<5)));
	} else if (PhyType == 0x161){ // KSZ9021
		fsbl_printf(DEBUG_GENERAL,"Detected KSZ9021 Ethernet PHY\n\r");
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xB, 0x8104); // write Reg 0x104
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xC, 0xF0F0); // set write data
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xB, 0x8105); // write Reg 0x105
		XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0xC, 0x0000); // set write data
	}

	// Issue a reset to phy
	Status  = XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr, 0x0, &PhyData);
	PhyData |= 0x8000;
	Status = XEmacPs_PhyWrite(EmacPsInstancePtr, PhyAddr, 0x0, PhyData);
	for (i=0; i<100000; i++);
	Status |= XEmacPs_PhyRead(EmacPsInstancePtr, PhyAddr, 0x0, &PhyData);
	if (Status != XST_SUCCESS)
	{
		fsbl_printf(DEBUG_GENERAL,"Error reset phy \n\r");
		return -1;
	} else {
		return 0;
	}
}



