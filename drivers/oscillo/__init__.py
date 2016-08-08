# -*- coding: utf-8 -*-

import time
import numpy as np

from koheron_tcp_client import command, write_buffer

class Oscillo(object):

    def __init__(self, client, wfm_size=8192):
        self.client = client
        self.wfm_size = wfm_size

    @command('OSCILLO','I')
    def set_n_avg_min(self, n_avg_min): pass

    @command('OSCILLO','I')
    def set_period(self, period): pass

    @command('OSCILLO')
    def get_counter(self):
        return self.client.recv_int(8, fmt='Q')

    @command('OSCILLO','II')
    def set_dac_period(self, dac_period0, dac_period1): pass

    @command('OSCILLO','II')
    def set_avg_period(self, avg_period0, avg_period1): pass

    @command('OSCILLO')
    def reset(self): pass

    @command('OSCILLO')
    def reset_acquisition(self): pass

    def set_dac(self, data, channel):
        @command('OSCILLO','IA')
        def set_dac_buffer(self, channel, array):
            pass
        data = np.uint32(np.mod(np.floor(8192 * data) + 8192, 16384) + 8192)
        set_dac_buffer(self, channel, data[::2] + (data[1::2] << 16))

    @command('OSCILLO', 'I')
    def get_dac_buffer(self, channel):
        return self.client.recv_buffer(self.wfm_size/2, data_type='uint32')

    @command('OSCILLO', '?')
    def set_averaging(self, avg_status):
        pass

    @command('OSCILLO')
    def get_num_average(self):
        return self.client.recv_uint32()

    @command('OSCILLO')
    def get_num_average_0(self):
        return self.client.recv_uint32()

    @command('OSCILLO')
    def get_num_average_1(self):
        return self.client.recv_uint32()

    @command('OSCILLO')
    def read_all_channels(self):
        return self.client.recv_buffer(2 * self.wfm_size, data_type='float32')

    def get_adc(self):
        data = self.read_all_channels()
        return np.reshape(data, (2, self.wfm_size))


    def test(self, verbose=True):
        if verbose:
            print('Testing OSCILLO driver')
        oscillo = self
        oscillo.reset()

        dac_data = np.arange(8192) / 8192.
        oscillo.set_dac(dac_data, 1)

        oscillo.set_dac(dac_data, 0)


        oscillo.set_dac(dac_data, 1)

        time.sleep(0.01)
        adc = oscillo.get_adc()
        assert(np.shape(adc) == (2, 8192))
        mean = np.mean(adc)
        std = np.std(adc)
        if verbose:
            print('Get adc: mean = {}, std = {}'.format(mean, std))
        assert(std > 0)
