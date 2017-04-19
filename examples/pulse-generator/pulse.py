#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from koheron import command

class Pulse(object):
    def __init__(self, client):
        self.client = client
        # self.n_pts = 16384
        self.n_pts = 8192
        self.fs = 125e6 # sampling frequency (Hz)

        self.adc = np.zeros((2, self.n_pts))
        self.dac = np.zeros((2, self.n_pts))

    @command()
    def trig_pulse(self):
        pass

    @command()
    def get_count(self):
        return self.client.recv_uint32()

    @command()
    def set_pulse_width(self, width):
        pass

    @command()
    def set_pulse_period(self, period):
        pass

    def set_dac(self):
        @command()
        def set_dac_data(self, data):
            pass
        dac_data_1 = np.uint32(np.mod(np.floor(8192 * self.dac[0, :]) + 8192, 16384) + 8192)
        dac_data_2 = np.uint32(np.mod(np.floor(8192 * self.dac[1, :]) + 8192, 16384) + 8192)
        set_dac_data(self, dac_data_1 + 65536 * dac_data_2)

    @command()
    def get_fifo_length(self):
        return self.client.recv_uint32()

    @command()
    def get_fifo_buffer(self):
        return self.client.recv_array(1024, dtype='uint32')

    @command()
    def reset_fifo(self):
        pass
