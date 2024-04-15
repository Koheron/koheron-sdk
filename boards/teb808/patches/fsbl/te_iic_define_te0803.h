/*
-- Company: 		Trenz Electronic
-- Engineer: 		Oleksandr Kiyenko / John Hartfiel
 */

#ifndef SRC_TE_IIC_DEFINE_H_
#define SRC_TE_IIC_DEFINE_H_

/* Board specific settings - General example*/

/* use own 'main' function */
// #define TE_STANDALONE

/* Define clock chip */
// #define CLOCK_SI5345
// #define NVM_CODE /* for SI5345: NVM writing is limited!,  you do it on your own risk!*/

#define CLOCK_SI5338


/* IIC interface type definition (use one that pass) */
// #define IIC_TYPE_AXI_IIC
// #define IIC_BASE_ADDRESS	XPAR_IIC_0_BASEADDR

//#define IIC_TYPE_ZYNQPS_IIC
//#define XIICPS_DEVICE_ID	XPAR_XIICPS_0_DEVICE_ID

#define IIC_TYPE_ZYNQUPS_IIC
#define XIICPS_DEVICE_ID	XPAR_XIICPS_0_DEVICE_ID

//#define IIC_TYPE_MCS_GPIO_IIC
//#define GPIO_SDA_PIN	0
//#define GPIO_SCL_PIN	1

//#define IIC_TYPE_AXI_GPIO_IIC
//#define GPIO_DEVICE_ID	XPAR_GPIO_0_DEVICE_ID
//#define GPIO_CHANNEL	0
//#define GPIO_SDA_PIN	0
//#define GPIO_SCL_PIN	1

//#define IIC_TYPE_PS_GPIO_IIC
//#define GPIO_DEVICE_ID	XPAR_XGPIOPS_0_DEVICE_ID
//#define GPIO_SDA_PIN	0
//#define GPIO_SCL_PIN	1

/* Define IIC clock speed */
#define IIC_SCLK_RATE			400000


/* UART interface type definition (use one that pass) */

// #define UART_TYPE_AXI_UARTLITE
// #define UART_DEVICE_ID			XPAR_AXI_UARTLITE_0_DEVICE_ID

//#define UART_TYPE_AXI_UART16550
//#define UART_DEVICE_ID			XPAR_UARTNS550_0_DEVICE_ID

//#define UART_TYPE_ZYNQPS_UART
//#define UART_DEVICE_ID			XPAR_XUARTPS_0_DEVICE_ID

#define UART_TYPE_ZYNQUPS_UART
#define UART_DEVICE_ID			XPAR_XUARTPS_0_DEVICE_ID

//#define UART_TYPE_NO_UART

/* Define UART baudrate */
#define UART_BAUDRATE			115200

#endif /* SRC_TE_IIC_DEFINE_H_ */
