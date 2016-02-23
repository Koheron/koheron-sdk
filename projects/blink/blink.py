#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from sampling import Sampling
from koheron_tcp_client import command, write_buffer

class Zx5(object):

    def __init__(self, client):
        wfm_size = 8192
        self.client = client
        self.open_zx5(wfm_size)
        self.n = wfm_size
        self.sampling = Sampling(wfm_size, 125e6)

        self.opened = True
        self.dac = np.zeros((2, self.sampling.n))

        self.failed = False

    def open_zx5(self, wfm_size):
        @command('ZX5')
        def open(self, wfm_size):
            pass

        open(self, wfm_size)

    def close(self):
        self.reset()

    @command('ZX5')
    def reset(self):
        pass

    @command('ZX5')
    def get_laser_power(self):
        power = self.client.recv_int(4)

        if math.isnan(power):
            print("Can't read laser power")
            self.failed = True

        return power


    @write_buffer('ZX5')
    def set_dac_buffer(self, data):
        pass

    def set_dac(self, warning=False, reset=False):
        if warning:
            if np.max(np.abs(self.dac)) >= 1:
                print('WARNING : dac out of bounds')
        dac_data_1 = np.mod(np.floor(8192 * self.dac[0, :]) + 8192,16384) + 8192
        dac_data_2 = np.mod(np.floor(8192 * self.dac[1, :]) + 8192,16384) + 8192
        self.set_dac_buffer(dac_data_1 + 65536 * dac_data_2)

        if reset:
            self.reset_acquisition()

    @command('ZX5')
    def get_bitstream_id(self):
        return self.client.recv_buffer(8, data_type='uint32')

    @command('ZX5')
    def reset_acquisition(self):
        pass
