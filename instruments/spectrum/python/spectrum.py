# -*- coding: utf-8 -*-
import time
import numpy as np
from koheron import command

class Spectrum(object):

    def __init__(self, client, wfm_size=4096):
        self.client = client
        self.wfm_size = wfm_size

        demod = np.zeros((2, self.wfm_size))
        demod[0, :] = 0.49 * (1 - np.cos(2 * np.pi * np.arange(self.wfm_size) / self.wfm_size))
        demod[1, :] = 0

        self.set_demod(demod)
        self.set_noise_floor_buffer(np.zeros(self.wfm_size))
        self.set_scale_sch(0)
        self.set_n_avg_min(0)
        self.reset_acquisition()

    @command()
    def reset(self): pass

    @command()
    def reset_acquisition(self): pass

    @command()
    def set_n_avg_min(self, n_avg_min): pass

    def set_dac(self, channels=[0,1]):
        @command(classname='Spectrum')
        def set_dac_buffer(self, channel, data):
            pass
        for channel in channels:
            data = np.int16(16384 * (self.dac[channel,:]))
            set_dac_buffer(self, channel, np.uint32(data[1::2] + data[::2] * 65536))

    @command()
    def get_dac_buffer(self, channel):
        return self.client.recv_array(self.wfm_size/2, dtype='uint32')

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
    def get_spectrum(self):
        return self.client.recv_array(self.wfm_size, dtype='float32')

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
    def set_averaging(self, avg_status): pass

    def get_peak_values(self):
        @command()
        def store_peak_fifo_data(self):
            return self.client.recv_uint32()

        self.peak_stream_length = store_peak_fifo_data(self)

        @command()
        def get_peak_fifo_data(self):
            return self.client.recv_array(self.peak_stream_length, dtype='uint32')

        return get_peak_fifo_data(self)

    @command()
    def get_peak_fifo_length(self):
        return self.client.recv_uint32()

    @command()
    def fifo_start_acquisition(self, acq_period): pass

    @command()
    def fifo_stop_acquisition(self): pass
