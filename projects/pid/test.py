from config import *

dvm.write(CONFIG, DDS_OFF, 100000000)

# FIFO

RDFR_OFF = int('0x18',0)
RDFO_OFF = int('0x1C',0)
RDFD_OFF = int('0x20',0)
RLR_OFF  = int('0x24',0)

def set_cic_rate(rate):
    dvm.write(CONFIG, CIC_RATE_OFF, rate)

def get_fifo_length():
    return (dvm.read(FIFO, RLR_OFF) - 2**31)/4


set_cic_rate(625)

for i in range(100):
    print get_fifo_length()

