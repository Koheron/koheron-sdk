#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from koheron import command

class AdcDacBram(object):
    def __init__(self, client):
        self.client = client
        self.dac_size = self.get_dac_size()
        self.dac = np.zeros((2, self.dac_size))
        self.adc_size = self.get_adc_size()
        self.adc = np.zeros((2, self.adc_size))

    @command()
    def get_dac_size(self):
        return self.client.recv_uint32()

    @command()
    def get_adc_size(self):
        return self.client.recv_uint32()

    @command()
    def trigger_acquisition(self):
        pass

    def set_dac(self):
        @command()
        def set_dac_data(self, data):
            pass
        # Conversion to two's complement:
        data1 = np.uint32(np.mod(np.floor(8192 * self.dac[0, :]) + 8192, 16384) + 8192)
        data2 = np.uint32(np.mod(np.floor(8192 * self.dac[1, :]) + 8192, 16384) + 8192)
        set_dac_data(self, data1 + (data2 << 16))

    @command()
    def get_adc(self):
        data = self.client.recv_array(self.adc_size, dtype='uint32')
        # Conversion to two's complement:
        self.adc[0,:] = (np.int32(data % 16384) - 8192) % 16384 - 8192
        self.adc[1,:] = (np.int32(data >> 16) - 8192) % 16384 - 8192

    @command(classname='ClockGenerator')
    def phase_shift(self, shift):
        pass

    @command(classname='ClockGenerator')
    def set_sampling_frequency(self, val):
        pass