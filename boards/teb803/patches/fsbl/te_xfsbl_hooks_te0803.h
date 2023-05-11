/******************************************************************************
*
* 
*

******************************************************************************/

/*****************************************************************************/
/**
*
* @file te_xfsbl_hooks_custom.h
*
*
******************************************************************************/
//rename to correct board name
#ifndef TE_XFSBL_HOOKS_TE0803_H
#define TE_XFSBL_HOOKS_TE0803_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#include "xil_types.h"
#include "xfsbl_hw.h"

#include "te_iic_platform.h"
#include "te_si5338.h"
#include "xparameters.h"
/************************** Constant Definitions *****************************/
#define USE_TE_PSU_FOR_SI_INIT //enable TE PSU to write SI on the correct place in the FSBL (Xilinx default PSU is deactivated)


#define GPIO_MIO31_MASK	0x00000020U
#define GPIO_MIO30_MASK	0x00000010U
#define ICM_CFG_VAL_PCIE	0X1U
#define DELAY_1_US			0x1U
#define DELAY_5_US			0x5U
#define DELAY_32_US			0x20U
#define DELAY_500_US	  0x500U 
#define DELAY_1000_US	  0x1000U 
#define DELAY_AFTER_US	0x2000U 
/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

/************************** Variable Definitions *****************************/
// for xsfbl_hooks.h
u32 TE_XFsbl_HookBeforeBSDownload_Custom(void );

u32 TE_XFsbl_HookAfterBSDownload_Custom(void );

u32 TE_XFsbl_HookBeforeHandoff_Custom(u32 EarlyHandoff);

u32 TE_XFsbl_HookBeforeFallback_Custom(void);

u32 TE_XFsbl_HookPsuInit_Custom(void);

// for xsfbl_board.h
u32 TE_XFsbl_BoardInit_Custom(void);


#ifdef __cplusplus
}
#endif

#endif  /* XFSBL_HOOKS_H */
