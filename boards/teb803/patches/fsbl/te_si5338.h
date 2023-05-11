/*
-- Company: 		Trenz Electronic
-- Engineer: 		Oleksandr Kiyenko / John Hartfiel
 */

#ifndef SRC_SI5338_H_
#define SRC_SI5338_H_

#define code

#include "te_iic_platform.h"
#ifdef CLOCK_SI5338


// #define SI5338_CHIP_ADDR		0x70

#define TEST_REG_ADDR			0x00

#define LOS_MASK_IN1IN2IN3		0x04
#define LOS_MASK				LOS_MASK_IN1IN2IN3
#define PLL_LOL					0x10
#define LOS_FDBK				0x08
#define LOS_CLKIN				0x04
#define SYS_CAL					0x01
#define LOCK_MASK				(PLL_LOL | LOS_CLKIN | SYS_CAL)
#define FCAL_OVRD_EN			0x80
#define SOFT_RESET				0x02
#define EOB_ALL					0x10
#define DIS_LOL					0x80


int si5338_version(unsigned char chip_addr);
int si5338_status_wait(unsigned char chip_addr);
int si5338_init(unsigned char chip_addr);

#endif /* CLOCK_SI5338 */
#endif /* SRC_SI5338_H_ */
