/*
-- Company: 		Trenz Electronic
-- Engineer: 		Oleksandr Kiyenko / John Hartfiel
 */
#ifndef IIC_PLATFORM_H_
#define IIC_PLATFORM_H_
#include "xparameters.h"

/* Include board specific settings */
// #include "te_iic_define.h"
#include "te_iic_define_te0803.h"
// #include "te_iic_define_te0820.h"
// #include "te_iic_define_te0712.h"

/* Enable extra Debug messages */
//#define DEBUG_MSG
/*----------------------------------------------------------------------------*/
int iic_init(void);
int iic_write8(unsigned char chip_addr, unsigned char reg_addr, unsigned char reg_val);
int iic_read8(unsigned char chip_addr, unsigned char reg_addr, unsigned char *reg_val);
int iic_write8_mask(unsigned char chip_addr, unsigned char reg_addr, unsigned char reg_val, unsigned char mask);
int iic_write16(unsigned char chip_addr, unsigned short reg_addr, unsigned char reg_val);
int iic_read16(unsigned char chip_addr, unsigned short reg_addr, unsigned char *reg_val);
void iic_delay(int delay_ms);
/*----------------------------------------------------------------------------*/
#ifdef IIC_TYPE_AXI_IIC
#include "xiic.h"
#endif /* IIC_TYPE_AXI_IIC */
/*----------------------------------------------------------------------------*/
#ifdef IIC_TYPE_ZYNQPS_IIC
#include "xiicps.h"
#include <unistd.h>
#include <stdio.h>
XIicPs I2cInstancePtr;
#endif /* IIC_TYPE_ZYNQPS_IIC */
/*----------------------------------------------------------------------------*/
#ifdef IIC_TYPE_ZYNQUPS_IIC
#include "xiicps.h"
#include <sleep.h>
#include <stdio.h>
XIicPs I2cInstancePtr;
#endif /* IIC_TYPE_ZYNQUPS_IIC */
/*----------------------------------------------------------------------------*/
#ifdef IIC_TYPE_MCS_GPIO_IIC
#include "xiomodule_l.h"
#include "xstatus.h"
#include "xio.h"
#define GPIO_OUT_REG	XPAR_IOMODULE_SINGLE_BASEADDR + XGO_OUT_OFFSET
#define GPIO_IN_REG		XPAR_IOMODULE_SINGLE_BASEADDR + XGI_IN_OFFSET
#endif /* IIC_TYPE_MCS_GPIO_IIC */
/*----------------------------------------------------------------------------*/
#ifdef IIC_TYPE_AXI_GPIO_IIC
#include "xgpio.h"
XGpio Gpio;
#endif /* IIC_TYPE_AXI_GPIO_IIC */
/*----------------------------------------------------------------------------*/
#ifdef IIC_TYPE_PS_GPIO_IIC
#include "xgpiops.h"
XGpioPs Gpio;
#endif /* IIC_TYPE_PS_GPIO_IIC */
/*----------------------------------------------------------------------------*/
#endif /* IIC_PLATFORM_H_ */
