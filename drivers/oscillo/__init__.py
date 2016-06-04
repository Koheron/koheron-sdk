# -*- coding: utf-8 -*-

import numpy as np

from koheron_tcp_client import command, write_buffer

class Oscillo(object):

    def __init__(self, client, wfm_size=8192):
        self.client = client
        self.wfm_size = wfm_size

        if self.open() < 0:
            print('Cannot open device OSCILLO')
       
    @command('OSCILLO')
    def open(self):
        return self.client.recv_int32()

    @command('OSCILLO','I')
    def set_n_avg_min(self, n_avg_min): pass

    @command('OSCILLO','I')
    def set_period(self, period): pass

    @command('OSCILLO')
    def reset(self): pass

    @command('OSCILLO')
    def reset_acquisition(self): pass

    def set_dac(self, data):
        @write_buffer('OSCILLO')
        def set_dac_buffer(self, data): 
            pass
        data1 = np.mod(np.floor(8192 * data[0, :]) + 8192,16384) + 8192
        data2 = np.mod(np.floor(8192 * data[1, :]) + 8192,16384) + 8192
        set_dac_buffer(self, data1 + data2 << 16)

    @command('OSCILLO', '?')
    def set_averaging(self, avg_status):
        pass

    @command('OSCILLO')
    def get_num_average(self):
        return self.client.recv_uint32()

    @command('OSCILLO')
    def read_all_channels(self):
        return self.client.recv_buffer(2 * self.wfm_size, data_type='float32')

    def get_adc(self):
        data = self.read_all_channels()
        return np.reshape(data, (2, self.wfm_size))

