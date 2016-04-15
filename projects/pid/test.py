from config import *

import time
import numpy as np
import matplotlib.pyplot as plt

freq = 0 # MHz
fs = 125e6 # Sampling frequency
dvm.write(CONFIG, DDS_OFF, np.floor(freq / fs * 2**32))

# FIFO

RDFR_OFF = int('0x18',0)
RDFO_OFF = int('0x1C',0)
RDFD_OFF = int('0x20',0)
RLR_OFF  = int('0x24',0)

def set_cic_rate(rate):
    dvm.write(CONFIG, CIC_RATE_OFF, rate)

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

    @command('PID')
    def reset_fifo(self):
        pass
 
    def read_fifo(self, npts_max=2**24):
        fifo_length = self.get_fifo_length()
        return fifo_length, self.get_fifo_data(min(fifo_length, npts_max))

    def read_npts_fifo(self, npts):
        data = np.zeros(npts)
        idx = 0
        while idx < npts:
            length, data_rcv = self.read_fifo(npts_max=npts-idx)
            data[idx:idx+length] = data_rcv
            idx += length
        return data


driver = Pid(client)

dec_factor = 1024
set_cic_rate(dec_factor)

n = 16384 * 8

driver.reset_fifo()

data = (driver.read_npts_fifo(n) - 2**23) % 2**24 - 2**23

t = dec_factor * np.arange(n)/fs

f_fft = np.fft.fftfreq(n) * fs / dec_factor

#plt.plot(t, data)

plt.semilogx(np.fft.fftshift(f_fft), np.fft.fftshift(10*np.log10(np.abs(np.fft.fft(data))**2)))

plt.show()
