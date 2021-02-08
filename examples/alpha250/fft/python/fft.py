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

    def set_demod(self, data):
        @command()
        def set_demod_buffer(self, data):
            pass
        data1 = np.uint32(np.mod(np.floor(32768 * data[0, :]) + 32768, 65536) + 32768)
        data2 = np.uint32(np.mod(np.floor(32768 * data[1, :]) + 32768, 65536) + 32768)
        set_demod_buffer(self, data1 + (data2 << 16))

    @command()
    def read_psd(self):
        return self.client.recv_array(self.n_pts//2, dtype='float32')

    @command()
    def read_psd_raw(self):
        return self.client.recv_array(self.n_pts//2, dtype='float32')

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

    @command()
    def set_fft_window(self, window_name):
        pass
