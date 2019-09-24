#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from koheron import command

class AdcBram(object):
    def __init__(self, client):
        self.client = client
        self.adc_size = self.get_adc_size()
        self.adc0 = np.zeros((2, self.adc_size))
        self.adc1 = np.zeros((2, self.adc_size))

    @command()
    def get_adc_size(self):
        return self.client.recv_uint32()

    @command()
    def trigger_acquisition(self):
        pass

    @command()
    def get_adc(self, adc):
        data = self.client.recv_array(self.adc_size, dtype='uint32')

        if adc == 0:
            # Conversion to two's complement:
            self.adc0[0,:] = (np.int32(data % 65536) - 32768) % 65536 - 32768
            self.adc0[1,:] = (np.int32(data >> 16) - 32768) % 65536 - 32768
        elif adc == 1:
            self.adc1[0,:] = (np.int32(data % 65536) - 32768) % 65536 - 32768
            self.adc1[1,:] = (np.int32(data >> 16) - 32768) % 65536 - 32768

    @command(classname='ClockGenerator')
    def set_reference_clock(self, clkin):
        pass

    @command(classname='ClockGenerator')
    def phase_shift(self, shift):
        pass

    @command(classname='ClockGenerator')
    def set_sampling_frequency(self, val):
        pass