from config import *

import time
import numpy as np
import matplotlib.pyplot as plt

freq = 10 # MHz
fs = 125e6 # Sampling frequency
dvm.write(CONFIG, DDS_OFF, np.floor(freq / fs * 2**32))

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

    def get_data(self):
        @command('PID')
        def store_fifo_data(self):
            return self.client.recv_int(4)

        self.fifo_stream_length = store_fifo_data(self)
        print "stream_length = " + str(self.fifo_stream_length)

        if self.fifo_stream_length > 0:
            @command('PID')
            def get_fifo_data(self):
                return self.client.recv_buffer(self.fifo_stream_length, data_type='uint32')

            return get_fifo_data(self)
        else:
            return []

    @command('PID')
    def fifo_stop_acquisition(self):
        pass

driver = Pid(client)

acq_period = 1000 # microseconds

print driver.fifo_get_acquire_status()
driver.fifo_start_acquisition(acq_period)
print driver.fifo_get_acquire_status()

set_cic_rate(625)

for i in range(100):
    print driver.get_fifo_length()
    print driver.get_data()

driver.fifo_stop_acquisition()
