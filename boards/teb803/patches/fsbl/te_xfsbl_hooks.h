/******************************************************************************
*
* 
*
******************************************************************************/

/*****************************************************************************/
/**
*
* @file te_xfsbl_hooks.h
*
*
******************************************************************************/
#ifndef TE_XFSBL_HOOKS_H
#define TE_XFSBL_HOOKS_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#include "xil_types.h"
#include "xfsbl_hw.h"
// rename and enable custom TE hooks
#include "te_xfsbl_hooks_te0803.h"

#define ENABLE_TE_HOOKS_PSU //TE_XFsbl_HookPsuInit
#define ENABLE_TE_BOARD // TE_XFsbl_BoardInit
// #define ENABLE_TE_HOOKS_BD //TE_XFsbl_HookBeforeBSDownload
// #define ENABLE_TE_HOOKS_AD //TE_XFsbl_HookAfterBSDownload
// #define ENABLE_TE_HOOKS_BH //TE_XFsbl_HookBeforeHandoff
// #define ENABLE_TE_HOOKS_BF // TE_XFsbl_HookBeforeFallback

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

/************************** Variable Definitions *****************************/
// for xsfbl_hooks.h/c
u32 TE_XFsbl_HookBeforeBSDownload(void );

u32 TE_XFsbl_HookAfterBSDownload(void );

u32 TE_XFsbl_HookBeforeHandoff(u32 EarlyHandoff);

u32 TE_XFsbl_HookBeforeFallback(void);

u32 TE_XFsbl_HookPsuInit(void);

// for xsfbl_board.h/c
u32 TE_XFsbl_BoardInit(void);


#ifdef __cplusplus
}
#endif

#endif  /* XFSBL_HOOKS_H */
