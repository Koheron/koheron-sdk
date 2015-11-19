from lase.core import KClient, DevMem, ZynqSSH
import numpy as np
import os
import matplotlib.pyplot as plt
import time

host = '192.168.1.12'
password = 'changeme'

# Load bitstream
ssh = ZynqSSH(host, password)
ssh.load_pl(os.path.join(os.getcwd(),'tmp/base_system.bit'))

client = KClient(host)
dvm = DevMem(client)

MAP_SIZE = 4096

# Base address of configuration register
CONFIG_ADDR = int('0x60000000',0)
CONFIG_SIZE = MAP_SIZE
# Offsets of configuration register
LED  = 0
PWM0 = 4
PWM1 = 8
PWM2 = 12
PWM3 = 16
AVG_COMP = 20
START_ACQ = 24


# Base address of AXI GPIO
GPIO_ADDR = int('0x41200000',0)
GPIO_SIZE = 16 * MAP_SIZE
# GPIO registers offsets (https://github.com/Koheron/zynq-sdk/issues/13)
GPIO_DATA  = 0
GPIO_TRI   = 4
GPIO2_DATA = 8
GPIO2_TRI  = 12

# Base address of DAC BRAM
DAC_ADDR = int('0x40000000',0)
DAC_SIZE = 8 * MAP_SIZE

# Base address of XADC Wizard
XADC_ADDR = int('0x43C00000',0)
XADC_SIZE = 16 * MAP_SIZE

# Base address of ADC1
ADC1_ADDR = int('0x42000000',0)
ADC1_SIZE = 8 * MAP_SIZE

# Ask Koheron server to open memory maps
CONFIG = dvm.add_memory_map(CONFIG_ADDR, CONFIG_SIZE)
GPIO   = dvm.add_memory_map(GPIO_ADDR  , GPIO_SIZE)
DAC    = dvm.add_memory_map(DAC_ADDR   , DAC_SIZE)
XADC   = dvm.add_memory_map(XADC_ADDR  , XADC_SIZE)
ADC1   = dvm.add_memory_map(ADC1_ADDR  , ADC1_SIZE)

# Set all GPIOs of channel 1 to output 
dvm.write(GPIO, GPIO_TRI, 0)

# Turn ON (3.3 V) the first 4 GPIOs of channel 1
val = 15
dvm.write(GPIO, GPIO_DATA, val)
assert dvm.read(GPIO, GPIO_DATA) == val

# Turn on LEDs 0, 1 and 3
dvm.write(CONFIG, LED, 11)

# Set Analog Output 0 to 0.9 V
dvm.write(CONFIG, PWM0, 512)

# Set DAC1 to 0.5 V
dac = np.zeros((2, 8192))
dac[0,:] = 0.5
dac[1,4096:4096+127] = 0.5
dac_data_1 = np.mod(np.floor(8192*dac[0,:]) + 8192,16384)+8192
dac_data_2 = np.mod(np.floor(8192*dac[1,:]) + 8192,16384)+8192
dvm.write_buffer(DAC, 0, dac_data_1 + 65536 * dac_data_2)

# Test ADC
dvm.write(CONFIG, AVG_COMP, 1*8187+1*2**13)
dvm.write(CONFIG, START_ACQ, 1)

a = dvm.read_buffer(ADC1, 0, 8192)
a = np.mod(a-2**31,2**32)-2**31
print np.mean(a)


dvm.write(CONFIG, START_ACQ, 1)
a = dvm.read_buffer(ADC1, 0, 8192)
a = np.mod(a-2**31,2**32)-2**31
plt.plot(a)

print np.sum(np.abs(a) > 20000)

plt.show()

