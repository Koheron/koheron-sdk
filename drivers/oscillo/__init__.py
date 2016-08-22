# -*- coding: utf-8 -*-

import time
import numpy as np

from koheron_tcp_client import command, write_buffer

class Oscillo(object):

    def __init__(self, client, wfm_size=8192):
        self.client = client
        self.wfm_size = wfm_size

    @command('Oscillo','I')
    def set_n_avg_min(self, n_avg_min): pass

    @command('Oscillo','I')
    def set_period(self, period): pass

    @command('Oscillo')
    def get_counter(self):
        return self.client.recv_int(8, fmt='Q')

    @command('Oscillo','II')
    def set_dac_period(self, dac_period0, dac_period1): pass

    @command('Oscillo','II')
    def set_avg_period(self, avg_period0, avg_period1): pass

    @command('Oscillo')
    def reset(self): pass

    @command('Oscillo')
    def reset_acquisition(self): pass

    def set_dac(self, data, channel):
        @command('Oscillo','IA')
        def set_dac_buffer(self, channel, array):
            pass
        data = np.uint32(np.mod(np.floor(8192 * data) + 8192, 16384) + 8192)
        set_dac_buffer(self, channel, data[::2] + (data[1::2] << 16))

    @command('Oscillo','IA')
    def set_dac_float(self, channel, array):
        assert len(array) == self.wfm_size

    @command('Oscillo', 'I')
    def get_dac_buffer(self, channel):
        return self.client.recv_array(self.wfm_size/2, dtype='uint32')

    @command('Oscillo', '?')
    def set_averaging(self, avg_status):
        pass

    @command('Oscillo','I')
    def get_num_average(self, channel):
        return self.client.recv_uint32()

    @command('Oscillo')
    def read_all_channels(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32')

    def get_adc(self):
        data = self.read_all_channels()
        return np.reshape(data, (2, self.wfm_size))
