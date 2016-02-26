#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np

from sampling import Sampling
from koheron_tcp_client import command, write_buffer

class DAC(object):
    """ Control the fast DAC.

    args:
        wfm_size: number of points in the waveform.
        client : instance of KClient connected to tcp-server.
    """
    def __init__(self, wfm_size, client):
        self.client = client
        self.open_dac(wfm_size)

        self.n = wfm_size
        self.sampling = Sampling(wfm_size, 125e6)
        self.data = np.zeros((2, self.sampling.n))

    def open_dac(self, wfm_size):
        @command('DAC')
        def open(self, wfm_size): pass

        open(self, wfm_size)

    def close(self):
        self.reset()

    @command('DAC')
    def reset(self): pass

    @write_buffer('DAC')
    def set_dac_buffer(self, data): pass

    def set_dac(self, warning=False, reset=False):
        if warning and np.max(np.abs(self.data)) >= 1:
            print('WARNING : dac out of bounds')
        dac_data_1 = np.mod(np.floor(8192 * self.data[0, :]) + 8192, 16384) + 8192
        dac_data_2 = np.mod(np.floor(8192 * self.data[1, :]) + 8192, 16384) + 8192
        self.set_dac_buffer(dac_data_1 + 65536 * dac_data_2)

        if reset:
            self.reset_acquisition()

    @command('DAC')
    def get_bitstream_id(self):
        return self.client.recv_buffer(8, data_type='uint32')

    @command('DAC')
    def reset_acquisition(self): pass