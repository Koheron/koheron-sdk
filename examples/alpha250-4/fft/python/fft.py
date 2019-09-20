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
        return np.array([self.get_control_parameters()[0], self.get_control_parameters()[1]])

    @command()
    def get_fft_size(self):
        return self.client.recv_uint32()

    @command()
    def get_control_parameters(self):
        return self.client.recv_tuple('ddIdd')

    @command()
    def get_cycle_index(self):
        return self.client.recv_tuple('II')

    @command()
    def set_input_channel(self, channel):
        pass

    @command()
    def read_psd(self, adc):
        return self.client.recv_array(self.n_pts/2, dtype='float32')

    @command()
    def read_psd_raw(self, adc):
        return self.client.recv_array(self.n_pts/2, dtype='float32')

    @command()
    def set_fft_window(self, window_name):
        pass
