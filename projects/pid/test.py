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
    return (dvm.read(FIFO, RLR_OFF) - 2**31) / 4

class Pid(object):

    def __init__(self, client):
        self.client = client
        if self.open() < 0:
            print "Cannot open driver"

    @command('PID')
    def open(self):
        return self.client.recv_int(4)

    @command('PID')
    def get_fifo_length(self):
        return self.client.recv_int(4)

    @command('PID')
    def store_fifo_data(self):
        return self.client.recv_int(4)

    @command('PID')
    def fifo_get_acquire_status(self):
        return self.client.recv_int(4)

    @command('PID')
    def fifo_start_acquisition(self, acq_period):
        pass

driver = Pid(client)

acq_period = 10000 # microseconds

print driver.fifo_get_acquire_status()

driver.fifo_start_acquisition(acq_period)

print driver.fifo_get_acquire_status()

set_cic_rate(625)

for i in range(100):
    print driver.get_fifo_length()#, get_fifo_length()

