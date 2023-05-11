/******************************************************************************
*
* 
*
******************************************************************************/

/*****************************************************************************/
/**
*
* @file te_xfsbl_hooks.c

******************************************************************************/
/***************************** Include Files *********************************/
#include "te_xfsbl_hooks.h"
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

/************************** Variable Definitions *****************************/
u32 TE_XFsbl_HookBeforeBSDownload(void )
{
	u32 Status = XFSBL_SUCCESS;

	/**
	 * Add the code here
	 */
#if defined(ENABLE_TE_HOOKS_BD)
	Status = TE_XFsbl_HookBeforeBSDownload_Custom();
#endif  

	return Status;
}


u32 TE_XFsbl_HookAfterBSDownload(void )
{
	u32 Status = XFSBL_SUCCESS;

	/**
	 * Add the code here
	 */
#if defined(ENABLE_TE_HOOKS_AD)
	Status = TE_XFsbl_HookAfterBSDownload_Custom();
#endif  

	return Status;
}

u32 TE_XFsbl_HookBeforeHandoff(u32 EarlyHandoff)
{
	u32 Status = XFSBL_SUCCESS;

	/**
	 * Add the code here
	 */
#if defined(ENABLE_TE_HOOKS_BH)
	Status = TE_XFsbl_HookBeforeHandoff_Custom(EarlyHandoff);
#endif  

	return Status;
}

/*****************************************************************************/
/**
 * This is a hook function where user can include the functionality to be run
 * before FSBL fallback happens
 *
 * @param none
 *
 * @return error status based on implemented functionality (SUCCESS by default)
 *
  *****************************************************************************/

u32 TE_XFsbl_HookBeforeFallback(void)
{
	u32 Status = XFSBL_SUCCESS;

	/**
	 * Add the code here
	 */
#if defined(ENABLE_TE_HOOKS_BF)
	Status = TE_XFsbl_HookBeforeFallback_Custom();
#endif  

	return Status;
}

/*****************************************************************************/
/**
 * This function facilitates users to define different variants of psu_init()
 * functions based on different configurations in Vivado. The default call to
 * psu_init() can then be swapped with the alternate variant based on the
 * requirement.
 *
 * @param none
 *
 * @return error status based on implemented functionality (SUCCESS by default)
 *
  *****************************************************************************/
#if defined(ENABLE_TE_HOOKS_PSU)  
# else 
u32 TE_XFsbl_PSU_Default(void)
{
  //copy from Xilinx default PSU, xilinx default PSU from Xilinx xfsbl_hooks must be deactivated when TE modified variant is used
    u32 Status;
  #ifdef XFSBL_ENABLE_DDR_SR
    u32 RegVal;
  #endif
    
  #ifdef XFSBL_ENABLE_DDR_SR
    /* Check if DDR is in self refresh mode */
    RegVal = Xil_In32(XFSBL_DDR_STATUS_REGISTER_OFFSET) &
      DDR_STATUS_FLAG_MASK;
    if (RegVal) {
      Status = (u32)psu_init_ddr_self_refresh();
    } else {
      Status = (u32)psu_init();
    }
  #else
    Status = (u32)psu_init();
  #endif
  
  	return Status;
}
#endif  


u32 TE_XFsbl_HookPsuInit(void)
{
	u32 Status = XFSBL_SUCCESS;

	/* Add the code here */
#if defined(ENABLE_TE_HOOKS_PSU)
	Status = TE_XFsbl_HookPsuInit_Custom();
# else 
   Status = TE_XFsbl_PSU_Default();
#endif  

	return Status;
}



/***for xsfbl_board.h***/
/*****************************************************************************/
/**
 * This function does board specific initialization.
 * If there isn't any board specific initialization required, it just returns.
 *
 * @param none
 *
 * @return
 * 		- XFSBL_SUCCESS for successful configuration
 * 		- errors as mentioned in xfsbl_error.h
 *
 *****************************************************************************/
u32 TE_XFsbl_BoardInit(void)
{
	u32 Status = XFSBL_SUCCESS;
  
#if defined(ENABLE_TE_BOARD)
	Status = TE_XFsbl_BoardInit_Custom();
#endif  
  

	return Status;
}
