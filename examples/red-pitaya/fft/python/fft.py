#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np
from enum import IntEnum

from koheron import command

class Window(IntEnum):
    RECT = 0
    HANN = 1
    FLAT_TOP = 2
    BLACKMAN_HARRIS = 3

class FFT(object):
    def __init__(self, client):
        self.client = client
        self.n_pts = self.get_fft_size()

    def get_fs(self):
        return self.get_control_parameters()[2]

    @command()
    def get_fft_size(self):
        return self.client.recv_uint32()

    @command()
    def get_cycle_index(self):
        return self.client.recv_tuple('II')

    @command()
    def set_offset(self, offset_real, offset_imag):
        pass

    @command()
    def set_input_channel(self, channel):
        pass

    @command()
    def set_fft_window(self, window_id):
        pass

    @command()
    def read_psd(self):
        return self.client.recv_array(self.n_pts/2, dtype='float32')

    @command()
    def read_psd_raw(self):
        return self.client.recv_array(self.n_pts/2, dtype='float32')

    # DDS

    @command()
    def set_dds_freq(self, channel, freq):
        pass

    @command()
    def get_control_parameters(self):
        return self.client.recv_tuple('dddIdd')

    @command()
    def get_adc_raw_data(self, n_avg):
        return self.client.recv_array(2, dtype='int32')

    # Demodulation

    @command(classname='Demodulator')
    def get_fifo_length(self):
        return self.client.recv_uint32()

    @command(classname='Demodulator')
    def get_vector(self, n_pts):
        return self.client.recv_vector(dtype='int32')
