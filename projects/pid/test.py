from config import *

import time
import numpy as np
import matplotlib.pyplot as plt

dvm.write(CONFIG, DDS_OFF, 0)

# FIFO

RDFR_OFF = int('0x18',0)
RDFO_OFF = int('0x1C',0)
RDFD_OFF = int('0x20',0)
RLR_OFF  = int('0x24',0)

def set_cic_rate(rate):
    dvm.write(CONFIG, CIC_RATE_OFF, rate)

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
        return (self.client.recv_int(4) - 2**31)/4

    @command('PID')
    def get_fifo_occupancy(self):
        return self.client.recv_int(4)

    @command('PID')
    def get_fifo_data(self, n_pts):
        return self.client.recv_buffer(n_pts, data_type='uint32')

    @command('')
    def reset_peak_fifo(self):
        pass
 
    def read_fifo(self):
        fifo_length = self.get_fifo_length()
        return self.get_fifo_data(fifo_length)        

driver = Pid(client)

set_cic_rate(128)

length = 16384 * 16
data = np.zeros(length)
t_prev = 0
idx = 0

while idx < length - 8192:
    data_rcv = (1.0 * driver.read_fifo() - 2**23) % 2**24 - 2**23
    n_pts = len(data_rcv)
    t_now = time.time()
    print n_pts, (t_now - t_prev)/n_pts, np.mean(data_rcv)
    data[idx:idx+n_pts] = data_rcv
    t_prev = t_now
    idx += n_pts

plt.plot(data)
plt.show()
