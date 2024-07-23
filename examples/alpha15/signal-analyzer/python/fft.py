#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from koheron import command

class FFT(object):
    def __init__(self, client):
        self.client = client
        self.n_pts = self.get_fft_size()

    @command()
    def get_fft_size(self):
        return self.client.recv_uint32()

    @command()
    def get_cycle_index(self):
        return self.client.recv_uint32()

    @command()
    def select_adc_channel(self, channel):
        pass

    @command()
    def set_operation(self, operation):
        pass

    @command()
    def read_psd(self):
        return self.client.recv_array(self.n_pts//2, dtype='float32')

    @command()
    def read_psd_raw(self):
        return self.client.recv_array(self.n_pts//2, dtype='float32')

    @command()
    def set_fft_window(self, window_name):
        pass

    @command()
    def set_raw_window(self, win):
        pass

    # LTC2387

    @command(classname='Ltc2387')
    def adc_raw_data(self, n_avg):
        return self.client.recv_array(2, dtype='int32')

    @command(classname='Ltc2387')
    def range_select(self, channel, range):
        pass

    # Clock generator

    @command(classname="ClockGenerator")
    def set_reference_clock(self, val):
        pass
