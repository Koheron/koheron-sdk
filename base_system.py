from lase.core import KClient, DevMem
import numpy as np

host = '192.168.1.12'
client = KClient(host)
dvm = DevMem(client)

# Base address of configuration register
CONFIG_ADDR = int('0x60000000',0)
# Offsets of configuration register
LED  = 0
PWM0 = 4
PWM1 = 8
PWM2 = 12
PWM3 = 16

# Base address of AXI GPIO
GPIO_ADDR = int('0x41200000',0)
# GPIO registers offsets (https://github.com/Koheron/zynq-sdk/issues/13)
GPIO_DATA  = 0
GPIO_TRI   = 4
GPIO2_DATA = 8
GPIO2_TRI  = 12

# Ask Koheron server to open memory maps
CONFIG = dvm.add_memory_map(CONFIG_ADDR, 1*4096)
GPIO = dvm.add_memory_map(GPIO_ADDR, 16*4096)


# Set all GPIOs of channel 1 to output 
dvm.write(GPIO, GPIO_TRI, 0)

# Turn ON (3.3 V) the first 4 GPIOs of channel 1
dvm.write(GPIO, GPIO_DATA, 15)

# Turn on LEDs 0, 1 and 3
dvm.write(CONFIG, LED, 11)

# Set Analog Output 0 to 0.9 V
dvm.write(CONFIG, PWM0, 512)


