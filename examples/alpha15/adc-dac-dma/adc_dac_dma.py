#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from koheron import command

class AdcDacDma(object):
    def __init__(self, client):
        self.n = 8 * 1024 * 1024
        self.client = client
        self.dac = np.zeros((self.n))
        self.adc = np.zeros((self.n))

    @command()
    def select_adc_channel(self, channel):
        pass

    @command()
    def set_dac_data(self, data):
        pass

    def set_dac(self, warning=False, reset=False):
        if warning:
            if np.max(np.abs(self.dac)) >= 1:
                print('WARNING : dac out of bounds')
        dac_data = np.uint32(np.mod(np.floor(32768 * self.dac) + 32768, 65536) + 32768)
        self.set_dac_data(dac_data[::2] + 65536 * dac_data[1::2])

    @command()
    def start_dma(self):
        pass

    @command()
    def stop_dma(self):
        pass

    @command()
    def get_adc_data(self):
        return self.client.recv_array(self.n//2, dtype='uint32')

    def _to_two_complement(self, data):
        nbits = 18
        nmax = 2**nbits
        return (np.int32(data % nmax) - nmax/2) % nmax - nmax/2

    def get_adc(self):
        data = self.get_adc_data()
        self.adc = self._to_two_complement(data)

    @command(classname='ClockGenerator')
    def set_reference_clock(self, clkin):
        pass

    @command(classname='Ltc2387')
    def adc_raw_data(self):
        return self.client.recv_array(2, dtype='int32')

    @command(classname='Ltc2387')
    def range_select(self, channel, range):
        pass
