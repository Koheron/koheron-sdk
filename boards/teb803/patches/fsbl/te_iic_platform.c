/*
-- Company: 		Trenz Electronic
-- Engineer: 		Oleksandr Kiyenko / John Hartfiel
 */
#include "te_iic_platform.h"
#include "te_uart.h"

/*----------------------------------------------------------------------------*/
#ifdef IIC_TYPE_AXI_IIC
int iic_init(void)
{
	int Status;
	u32 StatusReg;

	/* Initialize the IIC Core. */
	Status = XIic_DynInit(IIC_BASE_ADDRESS);
	if (Status != XST_SUCCESS) {
		p_printf(("IIC Controller Init failure\r\n"));
		return XST_FAILURE;
	}
	/* Make sure all the Fifo's are cleared and Bus is Not busy. */
	while (((StatusReg = XIic_ReadReg(IIC_BASE_ADDRESS,
				XIIC_SR_REG_OFFSET)) &
				(XIIC_SR_RX_FIFO_EMPTY_MASK |
				XIIC_SR_TX_FIFO_EMPTY_MASK |
				XIIC_SR_BUS_BUSY_MASK)) !=
				(XIIC_SR_RX_FIFO_EMPTY_MASK |
				XIIC_SR_TX_FIFO_EMPTY_MASK)) {
	}
	return Status;
}

int iic_write8(unsigned char chip_addr, unsigned char reg_addr, unsigned char reg_val)
{
	int Status;
	u8 SentByteCount;
	u8 WriteBuffer[2];

	WriteBuffer[0] = (u8) (reg_addr);
	WriteBuffer[1] = (u8) (reg_val);
	Status = XST_SUCCESS;

	SentByteCount = XIic_DynSend(IIC_BASE_ADDRESS, chip_addr, WriteBuffer, 2, XIIC_STOP);

	if(SentByteCount != 2){	// All bits
		Status = XST_FAILURE;
	}
	return Status;
}

int iic_read8(unsigned char chip_addr, unsigned char reg_addr, unsigned char *reg_val)
{
	u8 SentByteCount;
	u8 ReceivedByteCount;
	u8 WriteBuffer = reg_addr;

	#ifdef DEBUG_MSG
	p_printf(("iic_read8: addr 0x%04x\r\n", reg_addr));
	#endif
	SentByteCount = XIic_DynSend(IIC_BASE_ADDRESS, chip_addr, &WriteBuffer, 1, XIIC_STOP);
	if(SentByteCount != 1){
		return XST_FAILURE;
	}
	ReceivedByteCount = XIic_DynRecv(IIC_BASE_ADDRESS, chip_addr, reg_val, 1);
	#ifdef DEBUG_MSG
	p_printf(("iic_read8: received %d bytes = 0x%02x\r\n", ReceivedByteCount, reg_val[0]));
	#endif
	if(ReceivedByteCount != 1){
		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

void iic_delay(int delay_ms){
	volatile int i;
	for(i=0;i<(delay_ms*1000000);i++);
}

#endif /* IIC_TYPE_AXI_IIC */

/*----------------------------------------------------------------------------*/
#if defined(IIC_TYPE_ZYNQPS_IIC) || defined(IIC_TYPE_ZYNQUPS_IIC)
int iic_init(void)
{
	XIicPs_Config *I2cCfgPtr;
	int Status = XST_SUCCESS;

	I2cCfgPtr = XIicPs_LookupConfig(XIICPS_DEVICE_ID);

	if (I2cCfgPtr == NULL){
		p_printf(("IIC Controller lookup failure\r\n"));
		return XST_FAILURE;
	}

	Status = XIicPs_CfgInitialize(&I2cInstancePtr, I2cCfgPtr, I2cCfgPtr->BaseAddress);
	if (Status != XST_SUCCESS) {
		p_printf(("IIC Controller Init failure\r\n"));
		return XST_FAILURE;
	}

	XIicPs_SetSClk(&I2cInstancePtr, IIC_SCLK_RATE);
	return Status;
}

int iic_write8(unsigned char chip_addr, unsigned char reg_addr, unsigned char reg_val)
{
    u8 WriteBuffer[10];
	int Status;

	WriteBuffer[0] = reg_addr;
	WriteBuffer[1] = reg_val;
    Status = XIicPs_MasterSendPolled(&I2cInstancePtr, WriteBuffer, 2, chip_addr);
    if (Status != XST_SUCCESS) {
		return Status;
    }

    // Wait until bus is idle to start another transfer
    while (XIicPs_BusIsBusy(&I2cInstancePtr)) {};

	return XST_SUCCESS;
}

int iic_read8(unsigned char chip_addr, unsigned char reg_addr, unsigned char *data)
{
	u8 wr_data;
	wr_data = reg_addr;
	XIicPs_MasterSendPolled(&I2cInstancePtr, &wr_data, 1, chip_addr);
	XIicPs_MasterRecvPolled(&I2cInstancePtr, data, 1, chip_addr);
	while (XIicPs_BusIsBusy(&I2cInstancePtr));
	return 0;
}

void iic_delay(int delay_ms){
	usleep(delay_ms*1000);
}

#endif /* IIC_TYPE_ZYNQPS_IIC IIC_TYPE_ZYNQUPS_IIC */

/*----------------------------------------------------------------------------*/
#ifdef IIC_TYPE_MCS_GPIO_IIC


void gpio_set_pin(unsigned int pin, unsigned int value)
{
	static unsigned int gpio_shadow = 0;
	if(value == 0)
	{
		gpio_shadow &= ~(1 << pin);
	}
	else
	{
		gpio_shadow |= (1 << pin);
	}
	XIo_Out32(GPIO_OUT_REG, gpio_shadow);
}

u8 gpio_get_pin(unsigned int pin)
{
	if((XIo_In32(GPIO_IN_REG) & (1 << pin)) != 0)
	{
		return 1;
	}
	return 0; 
}

int iic_init(void)
{
	gpio_set_pin(GPIO_SDA_PIN, 1);
	gpio_set_pin(GPIO_SCL_PIN, 1);
  return 0; 
}

#endif /* IIC_TYPE_MCS_GPIO_IIC */

/*----------------------------------------------------------------------------*/
#ifdef IIC_TYPE_AXI_GPIO_IIC
int iic_init(void)
{
	int Status;
	u32 dir;
	Status = XGpio_Initialize(&Gpio, GPIO_DEVICE_ID);
	// SCL and SDA should have PULL UP
	dir = XGpio_GetDataDirection(&Gpio, GPIO_CHANNEL);
	dir |= (GPIO_SDA_PIN | GPIO_SCL_PIN);
	// Set Direction to IN
	XGpio_SetDataDirection(&Gpio, GPIO_CHANNEL, dir);
	// Set Values to 0
	XGpio_DiscreteClear(&Gpio, GPIO_CHANNEL, GPIO_SDA_PIN);
	XGpio_DiscreteClear(&Gpio, GPIO_CHANNEL, GPIO_SCL_PIN);

	return Status;
}

void gpio_set_pin(unsigned int pin, unsigned int value)
{
	u32 dir;
	dir = XGpio_GetDataDirection(&Gpio, GPIO_CHANNEL);
	if(value == 0){	// Set direction to OUT
		dir &= ~value;
		XGpio_SetDataDirection(&Gpio, GPIO_CHANNEL, dir);
	}
	else{			// Set direction to IN
		dir |= value;
		XGpio_SetDataDirection(&Gpio, GPIO_CHANNEL, dir);
	}
}

u8 gpio_get_pin(unsigned int pin)
{
	if(XGpio_DiscreteRead(&Gpio, GPIO_CHANNEL) | pin){
		return 1;
	}
	else{
		return 0;
	}
}

#endif /* IIC_TYPE_AXI_GPIO_IIC */

/*----------------------------------------------------------------------------*/
#ifdef IIC_TYPE_PS_GPIO_IIC

int iic_init(void)
{
	int Status;
	XGpioPs_Config *ConfigPtr;

	ConfigPtr = XGpioPs_LookupConfig(GPIO_DEVICE_ID);
	Status = XGpioPs_CfgInitialize(&Gpio, ConfigPtr, ConfigPtr->BaseAddr);
	// Set Direction to IN
	XGpioPs_SetDirectionPin(&Gpio, GPIO_SDA_PIN, 0);
	XGpioPs_SetDirectionPin(&Gpio, GPIO_SCL_PIN, 0);
	// Output disable
	XGpioPs_SetOutputEnablePin(&Gpio, GPIO_SDA_PIN, 0);
	XGpioPs_SetOutputEnablePin(&Gpio, GPIO_SCL_PIN, 0);
	// Set Values to 0
	XGpioPs_WritePin(&Gpio, GPIO_SDA_PIN, 0);
	XGpioPs_WritePin(&Gpio, GPIO_SCL_PIN, 0);

	return Status;
}

void gpio_set_pin(unsigned int pin, unsigned int value)
{
	if(value == 0){	// Set direction to OUT
		// Set Direction to IN
		XGpioPs_SetDirectionPin(&Gpio, value, 1);
		// Output enable
		XGpioPs_SetOutputEnablePin(&Gpio, value, 1);
	}
	else{			// Set direction to IN
		// Set Direction to IN
		XGpioPs_SetDirectionPin(&Gpio, value, 0);
		// Output disable
		XGpioPs_SetOutputEnablePin(&Gpio, value, 0);
	}
}

u8 gpio_get_pin(unsigned int pin)
{
	if(XGpioPs_ReadPin(&Gpio, Output_Pin) | pin)
	{
		return 1;
	}
	else{
		return 0;
	}
}

#endif /* IIC_TYPE_PS_GPIO_IIC */

/*----------------------------------------------------------------------------*/
/* Common functions for all GPIOs implementation */
#if defined(IIC_TYPE_MCS_GPIO_IIC) || defined(IIC_TYPE_AXI_GPIO_IIC) || defined(IIC_TYPE_PS_GPIO_IIC)

void iic_dly(){
	volatile int i;
	for(i=0;i<(1000);i++){
	}
}

void iic_delay(int delay_ms){
	volatile int i;
	for(i=0;i<(delay_ms*1000000);i++);
}

void iic_start(void)
{
	gpio_set_pin(GPIO_SDA_PIN, 1);	// Pull SDA High 
	gpio_set_pin(GPIO_SCL_PIN, 1);	// Pull SCL High 
	gpio_set_pin(GPIO_SDA_PIN, 0);	// pull SDA low
	iic_dly();
	gpio_set_pin(GPIO_SCL_PIN, 0);	// pull SCL low
	iic_dly();
}

void iic_stop(void)
{
	gpio_set_pin(GPIO_SDA_PIN, 0);	// Pull SDA low
	gpio_set_pin(GPIO_SCL_PIN, 0);	// Ensure SCL low
	iic_dly();
	gpio_set_pin(GPIO_SCL_PIN, 1);	// Pull SCL High 
	iic_dly();
	gpio_set_pin(GPIO_SDA_PIN, 1);	// Pull SDA High 
	iic_dly();
	gpio_set_pin(GPIO_SCL_PIN, 0);	// Pull SCL low
	iic_dly();
}

void iic_send_bit(u8 value)
{
	gpio_set_pin(GPIO_SCL_PIN, 0);	// Ensure that SCL is low
	gpio_set_pin(GPIO_SDA_PIN, value);	// Set data
	iic_dly();
	gpio_set_pin(GPIO_SCL_PIN, 1);	// Pull SCL High 
	iic_dly();
	gpio_set_pin(GPIO_SCL_PIN, 0);	// Pull SCL low
}

u8 iic_receive_bit()
{
	u8 rcv_data;
	
	gpio_set_pin(GPIO_SCL_PIN, 0);	// Ensure that SCL is low
	gpio_set_pin(GPIO_SDA_PIN, 1);	// Pull SDA High 
	iic_dly();
	gpio_set_pin(GPIO_SCL_PIN, 1);	// Pull SCL High 
	iic_dly();
	rcv_data = gpio_get_pin(GPIO_SDA_PIN);
	gpio_set_pin(GPIO_SCL_PIN, 0);	// Pull SCL low
	return rcv_data;
}

u8 iic_write(u8 value)
{
	u8 i;
	u8 shifter = value;
	
	for(i = 0; i < 8; i++)
	{
		iic_send_bit(shifter >> 7);
		shifter = shifter << 1;
	}
	return iic_receive_bit();
}

u8 iic_read(u8 ack)
{
	u8 i, shifter;
	
	for (i = 0; i < 8; i++)		// loop through each bit
	{
		shifter = shifter << 1;
		shifter |= iic_receive_bit();
	}
	iic_send_bit(ack);			// Send ACK/NACK
	return shifter;
}

int iic_write8(unsigned char chip_addr, unsigned char reg_addr, unsigned char reg_val)
{
	int Status = 0;
	
	iic_start();
	Status = iic_write(chip_addr << 1);		// Chip addr & Write 
	if(Status != 0)							// No ACK from chip
	{
		p_printf(("IIC Write: no ACK from Slave\r\n"));
		return 1;
	}
	Status = iic_write(reg_addr);			// Register address
	if(Status != 0)							// No ACK from chip
	{
		p_printf(("IIC Write: no ACK from Slave\r\n"));
		return 1;
	}
	Status = iic_write(reg_val);			// Register data
	if(Status != 0)							// No ACK from chip
	{
		p_printf(("IIC Write: no ACK from Slave\r\n"));
		return 1;
	}
	iic_stop();
	return 0;
}


int iic_read8(unsigned char chip_addr, unsigned char reg_addr, unsigned char *data)
{
	int Status = 0;
	
	iic_start();
	Status = iic_write(chip_addr << 1);		// Chip addr & Write 
	if(Status != 0)							// No ACK from chip
	{
		p_printf(("IIC Write: no ACK from Slave\r\n"));
		return 1;
	}
	Status = iic_write(reg_addr);			// Register address
	if(Status != 0)							// No ACK from chip
	{
		p_printf(("IIC Write: no ACK from Slave\r\n"));
		return 1;
	}
	iic_start();							// Repeated start
	Status = iic_write((chip_addr << 1) | 0x01);	// Chip addr & Read
	if(Status != 0)							// No ACK from chip
	{
		return 1;
	}
	*data = iic_read(1);						// NACK
	iic_stop();
	return 0;
}

#endif

/*----------------------------------------------------------------------------*/
/* Platform independent functions */
int iic_write8_mask(unsigned char chip_addr, unsigned char reg_addr, unsigned char reg_val, unsigned char mask)
{
	int Status;
	u8 rd_val;

	if(mask == 0xFF){				// All bits
		iic_write8(chip_addr, reg_addr, reg_val);
	}
	else{							// Write by mask
		Status = iic_read8(chip_addr, reg_addr, &rd_val);
		if(Status != XST_SUCCESS){
			return Status;
		}
		rd_val &= ~mask;			// Clear bits to write
		rd_val |= reg_val & mask;	// Set bits by mask
		Status = iic_write8(chip_addr, reg_addr, rd_val);
		if(Status != XST_SUCCESS){
			return Status;
		}
	}
	return XST_SUCCESS;
}

int iic_write16(unsigned char chip_addr, unsigned short reg_addr, unsigned char reg_val)
{
	int Status;
	u8 page, addr;
	static int _last_page = -1;

	page = (u8) (reg_addr >> 8);
	#ifdef DEBUG_MSG
	p_printf(("iic_write16: addr 0x%04x data 0x%02x page 0x%02x (0x%02x) addr 0x%02x [%d]\r\n", reg_addr, reg_val, page, _last_page, (reg_addr & 0xFF), total++));
	#endif

	if (_last_page < 0 || _last_page != page) {		// New page
		#ifdef DEBUG_MSG
		p_printf(("p\r\n"));
		#endif
		Status = iic_write8(chip_addr, 0x01, page);
		if (Status != XST_SUCCESS) {
			p_printf(("iic_write16 page write failure\r\n"));
			return XST_FAILURE;
		}
	}
	_last_page = page;

	#ifdef DEBUG_MSG
	p_printf(("r\r\n"));
	#endif
	addr = (u8) reg_addr & 0xFF;
	Status = iic_write8(chip_addr, addr, reg_val);
	if (Status != XST_SUCCESS) {
		p_printf(("iic_write16 reg write failure\r\n"));
		return XST_FAILURE;
	}
	return Status;
}

int iic_read16(unsigned char chip_addr, unsigned short reg_addr, unsigned char *reg_val)
{
	int Status;
	u8 page, addr;
	static int _last_page = -1;

	page = (u8) (reg_addr >> 8);
	#ifdef DEBUG_MSG
	p_printf(("iic_read16: addr 0x%04x page 0x%02x (0x%02x) addr 0x%02x\r\n", reg_addr, page, _last_page, (reg_addr & 0xFF)));
	#endif

	if (_last_page < 0 || _last_page != page) {		// New page
		#ifdef DEBUG_MSG
		p_printf(("p\r\n"));
		#endif
		Status = iic_write8(chip_addr, 0x01, page);
		if (Status != XST_SUCCESS) {
			p_printf(("iic_read16 page write failure\r\n"));
			return XST_FAILURE;
		}
	}
	_last_page = page;

	#ifdef DEBUG_MSG
	p_printf(("r\r\n"));
	#endif
	addr = (u8) reg_addr & 0xFF;
	Status = iic_read8(chip_addr, addr, reg_val);
	if (Status != XST_SUCCESS) {
		p_printf(("iic_read16 reg read failure\r\n"));
		return XST_FAILURE;
	}
	return Status;
}
/*----------------------------------------------------------------------------*/
