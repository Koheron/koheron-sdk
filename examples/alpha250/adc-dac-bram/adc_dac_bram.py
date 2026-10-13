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
        self.checked_capture = any(driver['class'] == 'AdcDacBram' and
            any(function['name'] == 'get_adc_checked' for function in driver['functions'])
            for driver in client.commands)

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
        data1 = np.uint32(np.mod(np.floor(32768 * self.dac[0, :]) + 32768, 65536) + 32768)
        data2 = np.uint32(np.mod(np.floor(32768 * self.dac[1, :]) + 32768, 65536) + 32768)
        set_dac_data(self, data1 + (data2 << 16))

    @command(funcname='get_adc')
    def _get_adc(self):
        return self.client.recv_array(self.adc_size, dtype='uint32')

    @command(funcname='get_adc_checked')
    def _get_adc_checked(self):
        self.client.check_ret_tuple()
        valid = self.client.recv(fmt='?')
        data = np.frombuffer(self.client.recv_all(self.adc_size * 4), dtype='<u4')
        return valid, data

    def get_adc(self):
        if self.checked_capture:
            valid, data = self._get_adc_checked()
            if not valid:
                raise RuntimeError('BRAM acquisition timed out')
        else:
            data = self._get_adc()
        # Conversion to two's complement:
        self.adc[0,:] = (np.int32(data % 65536) - 32768) % 65536 - 32768
        self.adc[1,:] = (np.int32(data >> 16) - 32768) % 65536 - 32768

    @command(classname='ClockGenerator')
    def phase_shift(self, shift):
        pass

    @command(classname='ClockGenerator')
    def set_sampling_frequency(self, val):
        pass

    @command(classname="ClockGenerator")
    def set_reference_clock(self, val):
        pass
