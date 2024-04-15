/*
-- Company: 		Trenz Electronic
-- Engineer: 		Oleksandr Kiyenko / John Hartfiel
 */

#include "te_si5338.h"

#ifdef CLOCK_SI5338
#include "te_uart.h"
#include "te_Si5338-Registers.h"

#define DELAY_AFTER_PLL_CONFIG_US	0x100000U 

int si5338_version(unsigned char chip_addr){
	u8 reg_val;
	int Status;
  
    Status = iic_read8( chip_addr, 2, &reg_val);
    xil_printf("SI53%i",reg_val);
    Status = iic_read8( chip_addr, 0, &reg_val);
    if (reg_val==0) {
      xil_printf("-A\r\n");
    } else if (reg_val==1) {
      xil_printf("-B\r\n");
    } else {
      xil_printf("-%x\r\n",reg_val);
    }

	  return Status;
}

int si5338_status_wait(unsigned char chip_addr){
	u8 reg_val;
	unsigned int  cnt=0, tmp;
	int Status;
  
  (void)usleep(0x5U);
  Status = iic_read8( chip_addr, 218, &reg_val);
  
  tmp = 1;
  // Wait until internal calibration is not busy
  while (tmp ==1) {
    cnt++ ;
    Status = iic_read8( chip_addr, 218, &reg_val);
    tmp =((reg_val) & (0x01));
    if ((cnt % 1000) == 0) {
      xil_printf("Status 218:0x%x (...waiting for calibration...$i).\r",reg_val,cnt);
    }
  }
   //sleep need for PCIe
  (void)usleep(DELAY_AFTER_PLL_CONFIG_US);
    
  Status = iic_read8( chip_addr, 218, &reg_val);
  xil_printf("PLL Status Register 218:0x%x.\r\n",reg_val);

	  return Status;
}



int si5338_init(unsigned char chip_addr)
{
	int i;
	u8 reg_val;
	Reg_Data rd;
	int Status;

    // p_printf(("Si5338 Init Start.\r\n"));
    // iic_init();
    p_printf(("Si5338 Init Registers Write.\r\n"));

	// I2C Programming Procedure
	iic_write8( chip_addr, 246, 0x01);					//Hard reset
	// Disable Outputs
	iic_write8_mask( chip_addr, 230, EOB_ALL, EOB_ALL);	// EOB_ALL = 1
	// Pause LOL
	iic_write8_mask( chip_addr, 241, DIS_LOL, DIS_LOL);	// DIS_LOL = 1
	// Write new configuration to device accounting for the write-allowed mask
	for(i=0; i<NUM_REGS_MAX; i++){
		rd = Reg_Store[i];
		iic_write8_mask( chip_addr, rd.Reg_Addr, rd.Reg_Val, rd.Reg_Mask);
	}
	// Validate clock input status
//	reg_val = iic_read8( chip_addr , 218) & LOS_MASK;
	do{
		Status = iic_read8( chip_addr , 218, &reg_val);
        if(Status != XST_SUCCESS) {
            p_printf(("si5338_init: Can't read register\r\n"));
            return Status;
        }
	}
	while((reg_val & LOS_MASK) != 0);

	// Configure PLL for locking
	iic_write8_mask( chip_addr, 49, 0, FCAL_OVRD_EN);	//FCAL_OVRD_EN = 0
	// Initiate Locking of PLL
	iic_write8( chip_addr, 246, SOFT_RESET);			//SOFT_RESET = 1
	iic_delay(25);											// Wait 25 ms
	// Restart LOL
	iic_write8_mask( chip_addr, 241, 0, DIS_LOL);		// DIS_LOL = 0
	iic_write8( chip_addr, 241, 0x65);				// Set reg 241 = 0x65
	// Confirm PLL lock status
	do{
		Status = iic_read8( chip_addr, 218, &reg_val);
        if(Status != XST_SUCCESS) {
            p_printf(("si5338_init: Can't read register\r\n"));
            return Status;
        }
	}
	while((reg_val & LOCK_MASK) != 0);
	//copy FCAL values to active registers
	Status = iic_read8( chip_addr, 237, &reg_val);
	iic_write8_mask( chip_addr, 47, reg_val, 0x03);	// 237[1:0] to 47[1:0]
	Status = iic_read8( chip_addr, 236, &reg_val);
	iic_write8( chip_addr, 46, reg_val);	// 236[7:0] to 46[7:0]
	Status = iic_read8( chip_addr, 235, &reg_val);
	iic_write8( chip_addr, 45, reg_val);	// 235[7:0] to 45[7:0]
	iic_write8_mask( chip_addr, 47, 0x14, 0xFC);		// Set 47[7:2] = 000101b
	// Set PLL to use FCAL values
	iic_write8_mask( chip_addr, 49, FCAL_OVRD_EN, FCAL_OVRD_EN);	//FCAL_OVRD_EN = 1
	// Enable Outputs
	iic_write8( chip_addr, 230, 0x00);					//EOB_ALL = 0
    p_printf(("Si5338 Init Complete\r\n"));
	return XST_SUCCESS;
}

#endif
