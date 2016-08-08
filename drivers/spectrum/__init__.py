# -*- coding: utf-8 -*-
import time
import numpy as np
from koheron_tcp_client import command, write_buffer

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

    @command('SPECTRUM')
    def reset(self): pass

    @command('SPECTRUM')
    def reset_acquisition(self): pass

    @command('SPECTRUM','I')
    def set_n_avg_min(self, n_avg_min): pass

    def set_dac(self, data, channel):
        @write_buffer('SPECTRUM','I')
        def set_dac_buffer(self, data, channel): 
            pass
        data = np.uint32(np.mod(np.floor(8192 * data) + 8192,16384) + 8192)
        set_dac_buffer(self, data[::2] + data[1::2] * 65536, channel)

    @command('SPECTRUM', 'I')
    def set_scale_sch(self, scale_sch):
        pass

    @command('SPECTRUM', 'II')
    def set_offset(self, offset_real, offset_imag):
        pass

    def set_demod(self, data):
        @write_buffer('SPECTRUM')
        def set_demod_buffer(self, data): 
            pass
        data1 = np.mod(np.floor(8192 * data[0, :]) + 8192,16384) + 8192
        data2 = np.mod(np.floor(8192 * data[1, :]) + 8192,16384) + 8192
        set_demod_buffer(self, data1 + data2 * 2**16)

    @write_buffer('SPECTRUM', format_char='f', dtype=np.float32)
    def set_noise_floor_buffer(self, data):
        pass
        
    @command('SPECTRUM')
    def get_spectrum(self):
        return self.client.recv_buffer(self.wfm_size, data_type='float32')

    @command('SPECTRUM')
    def get_num_average(self):
        return self.client.recv_uint32()

    @command('SPECTRUM')
    def get_peak_address(self):
        return self.client.recv_uint32()

    @command('SPECTRUM')
    def get_peak_maximum(self):
        return self.client.recv_int(4, fmt='f')

    @command('SPECTRUM', 'II')
    def set_address_range(self, address_low, address_high):
        pass

    @command('SPECTRUM', '?')
    def set_averaging(self, avg_status): pass

    def get_peak_values(self):
        @command('SPECTRUM')
        def store_peak_fifo_data(self):
            return self.client.recv_uint32()

        self.peak_stream_length = store_peak_fifo_data(self)

        @command('SPECTRUM')
        def get_peak_fifo_data(self):
            return self.client.recv_buffer(self.peak_stream_length, data_type='uint32')

        return get_peak_fifo_data(self)

    @command('SPECTRUM')
    def get_peak_fifo_length(self):
        return self.client.recv_uint32()

    @command('SPECTRUM', 'I')
    def fifo_start_acquisition(self, acq_period): pass

    @command('SPECTRUM')
    def fifo_stop_acquisition(self): pass

    def test(self):
        self.reset()
        time.sleep(0.1)
        assert(np.mean(self.get_spectrum()) > 0)
        print(self.get_num_average())
