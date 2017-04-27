# -*- coding: utf-8 -*-
import time
import numpy as np
from koheron import command

class Spectrum(object):

    def __init__(self, client):
        self.client = client

    @command()
    def reset(self):
        pass

    @command()
    def reset_acquisition(self):
        pass

    @command()
    def set_num_average_min(self, n_avg_min):
        pass

    @command()
    def set_scale_sch(self, scale_sch):
        pass

    @command()
    def set_offset(self, offset_real, offset_imag):
        pass

    def set_demod(self, data):
        @command()
        def set_demod_buffer(self, data):
            pass
        data1 = np.uint32(np.mod(np.floor(8192 * data[0, :]) + 8192,16384) + 8192)
        data2 = np.uint32(np.mod(np.floor(8192 * data[1, :]) + 8192,16384) + 8192)
        set_demod_buffer(self, data1 + data2 * 2**16)

    def set_noise_floor_buffer(self, data):
        @command()
        def set_noise_floor_buffer(self, data):
            pass
        set_noise_floor_buffer(self, np.float32(data))

    @command()
    def get_decimated_data(self, decim_factor, index_low, index_high):
        decimated_data = self.client.recv_vector(dtype='float32')
        return decimated_data

    @command()
    def get_num_average(self):
        return self.client.recv_uint32()

    @command()
    def get_peak_address(self):
        return self.client.recv_uint32()

    @command()
    def get_peak_maximum(self):
        return self.client.recv_float()

    @command()
    def set_address_range(self, address_low, address_high):
        pass

    @command()
    def set_average(self, is_average):
        pass

    @command()
    def get_peak_fifo_data(self):
        return self.client.recv_vector(dtype='uint32')
