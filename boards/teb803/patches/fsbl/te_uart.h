/*
-- Company: 		Trenz Electronic
-- Engineer: 		Oleksandr Kiyenko / John Hartfiel
 */
#ifndef SRC_TE_UART_H_
#define SRC_TE_UART_H_

#include "te_iic_platform.h"

u8 uart_read_char(void);

/*----------------------------------------------------------------------------*/
#ifdef UART_TYPE_AXI_UARTLITE
#include "xil_printf.h"
#include "xuartlite.h"
#define p_printf(x)	xil_printf x
XUartLite UartLite;
#endif /* UART_TYPE_AXI_UARTLITE */
/*----------------------------------------------------------------------------*/
#ifdef UART_TYPE_AXI_UART16550
#include "xil_printf.h"
#include "xuartns550.h"
#include "xuartns550_i.h"
#define p_printf(x)	xil_printf x
XUartNs550 UartNs550;
#endif /* UART_TYPE_AXI_UART16550 */
/*----------------------------------------------------------------------------*/
#if defined(UART_TYPE_ZYNQPS_UART) || defined(UART_TYPE_ZYNQUPS_UART)
#include "xil_printf.h"
#include "xuartps.h"
#define p_printf(x)	xil_printf x
XUartPs Uart_Ps;
#endif /* UART_TYPE_ZYNQPS_UART */
/*----------------------------------------------------------------------------*/
#ifdef UART_TYPE_NO_UART
#define p_printf(x)
#endif /* UART_TYPE_NO_UART */
/*----------------------------------------------------------------------------*/

#endif /* SRC_TE_UART_H_ */
