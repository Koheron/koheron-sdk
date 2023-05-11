/*
-- Company: 		Trenz Electronic
-- Engineer: 		Oleksandr Kiyenko / John Hartfiel
 */
#include "te_uart.h"

/*----------------------------------------------------------------------------*/
#ifdef UART_TYPE_AXI_UARTLITE
u8 uart_read_char(void){
	u8 u;
    XUartLite_Initialize(&UartLite, UART_DEVICE_ID);
    while(!XUartLite_Recv(&UartLite, &u, 1)){
    }
	return u;
}
#endif /* UART_TYPE_AXI_UARTLITE */
/*----------------------------------------------------------------------------*/
#ifdef UART_TYPE_AXI_UART16550
u8 uart_read_char(void){
	u8 u;
	XUartNs550_Initialize(&UartNs550, UART_DEVICE_ID);
	XUartNs550_SetBaudRate(&UartNs550, UART_BAUDRATE);
    while(!XUartNs550_Recv(&UartNs550, &u, 1)){
    }
	return u;
}
#endif /* UART_TYPE_AXI_UART16550 */
/*----------------------------------------------------------------------------*/
#if defined(UART_TYPE_ZYNQPS_UART) || defined(UART_TYPE_ZYNQUPS_UART)
u8 uart_read_char(void){
	u8 u;
	XUartPs_Config *Config;
	Config = XUartPs_LookupConfig(UART_DEVICE_ID);
	XUartPs_CfgInitialize(&Uart_Ps, Config, Config->BaseAddress);
	XUartPs_SetBaudRate(&Uart_Ps, UART_BAUDRATE);
	while(!XUartPs_Recv(&Uart_Ps, &u, 1)){
	}
	return u;
}
#endif /* UART_TYPE_ZYNQPS_UART */
/*----------------------------------------------------------------------------*/
#ifdef UART_TYPE_NO_UART
u8 uart_read_char(void){
	return 0;
}
#endif /* UART_TYPE_NO_UART */
/*----------------------------------------------------------------------------*/
