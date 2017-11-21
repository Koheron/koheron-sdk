#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from koheron import command

class Oscillo(object):
    def __init__(self, client):
        self.client = client
        self.wfm_size = 8192
        self.sampling_rate = 125e6
        self.t = np.arange(self.wfm_size)/self.sampling_rate
        self.dac = np.zeros((2, self.wfm_size))

        self.adc = np.zeros((2, self.wfm_size))
        self.spectrum = np.zeros((2, int(self.wfm_size / 2)))
        self.avg_spectrum = np.zeros((2, int(self.wfm_size / 2)))

    @command()
    def set_dac_periods(self, period0, period1):
        ''' Select the periods played on each address generator
        ex: self.set_dac_periods(8192, 4096)
        '''
        pass

    @command()
    def set_num_average_min(self, num_average_min):
        ''' Set the minimum of averages that will be computed on the FPGA
        The effective number of averages is >= num_average_min.
        '''
        pass

    @command()
    def set_average_period(self, average_period):
        ''' Set the period of the averaging module and reset the module.
        '''
        self.period = average_period

    @command()
    def set_average(self, is_average):
        ''' is_average = True enables averaging. '''
        pass

    @command()
    def get_num_average(self, channel):
        ''' Get the number of averages corresponding to the last acquisition. '''
        num_average = self.client.recv_uint32()
        return num_average

    @command()
    def get_decimated_data(self, decim_factor, index_low, index_high):
        decimated_data = self.client.recv_vector(dtype='float32')
        return decimated_data

    def get_adc(self):
        self.adc = np.reshape(self.get_decimated_data(1, 0, self.wfm_size), (2, self.wfm_size))

    def get_spectrum(self):
        fft_adc = np.fft.fft(self.adc, axis=1)
        self.spectrum = fft_adc[:, 0:self.wfm_size / 2]

    def get_avg_spectrum(self, n_avg=1):
        self.avg_spectrum = np.zeros((2, int(self.wfm_size / 2)))
        for i in range(n_avg):
            self.get_adc()
            fft_adc = np.abs(np.fft.fft(self.adc, axis=1))
            self.avg_spectrum += fft_adc[:, 0:int(self.wfm_size / 2)]
        self.avg_spectrum /= n_avg

    @command()
    def reset_acquisition(self):
        pass

    @command(funcname='reset')
    def reset_dac(self):
        pass

    def reset(self):
        self.reset_dac()

    # Modulation

    def set_dac(self, channels=[0,1]):
        """ Write the BRAM corresponding on the selected channels
        (dac0 or dac1) with the array stored in self.dac[channel,:].
        ex: self.set_dac(channel=[0])
        """
        @command(classname='Modulation')
        def set_dac_buffer(self, channel, arr):
            pass
        for channel in channels:
            data = np.int16(16384 * (self.dac[channel,:]))
            set_dac_buffer(self, channel, np.uint32(data[1::2] + data[::2] * 65536))

    @command(classname='Modulation')
    def get_modulation_status(self):
        return self.client.recv_tuple('IIffffff')

    @command(classname='Modulation')
    def set_waveform_type(self, channel, wfm_type):
        pass

    @command(classname='Modulation')
    def set_dac_amplitude(self, channel, amplitude_value):
        pass

    @command(classname='Modulation')
    def set_dac_frequency(self, channel, frequency_value):
        pass

    @command(classname='Modulation')
    def set_dac_offset(self, channel, frequency_value):
        pass