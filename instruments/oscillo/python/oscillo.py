#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from koheron import command

class Oscillo(object):
    def __init__(self, client, verbose=False):
        self.client = client
        self.wfm_size = 8192   
        self.avg_on = False

        self.period = self.wfm_size
        self.adc = np.zeros((2, self.wfm_size))
        self.spectrum = np.zeros((2, self.wfm_size / 2))
        self.avg_spectrum = np.zeros((2, self.wfm_size / 2))
        self.dac = np.zeros((2, self.wfm_size))

    @command()
    def set_dac_periods(self, period0, period1):
        ''' Select the periods played on each address generator
        ex: self.set_dac_periods(8192, 4096)
        '''
        pass

    @command()
    def set_n_avg_min(self, n_avg_min): 
        ''' Set the minimum of averages that will be computed on the FPGA
        The effective number of averages is >= n_avg_min.
        '''
        pass

    @command()
    def set_avg_period(self, avg_period):
        ''' Set the period of the averaging module and reset the module.
        '''
        self.period = avg_period

    def set_dac(self, channels=[0,1]):
        ''' Write the BRAM corresponding on the selected channels
        (dac0 or dac1) with the array stored in self.dac[channel,:].
        ex: self.set_dac(channel=[0])
        '''
        @command()
        def set_dac_buffer(self, channel, arr):
            pass
        for channel in channels:
            data = np.uint32(np.mod(np.floor(8192 * self.dac[channel,:]) + 8192, 16384) + 8192)
            set_dac_buffer(self, channel, data[::2] + data[1::2] * 65536)

    @command()
    def set_dac_float(self, channel, arr):
        pass

    @command()
    def get_dac_buffer(self, channel):
        return self.client.recv_array(self.wfm_size / 2, dtype='uint32')

    @command()
    def set_averaging(self, avg_status):
        ''' avg_status = True enables averaging. '''
        pass

    @command()
    def get_num_average(self, channel):
        ''' Get the number of averages corresponding to the last acquisition. '''
        n_avg = self.client.recv_uint32()
        return n_avg

    @command()
    def read_all_channels(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32')

    def get_adc(self):
        ''' Read adc data and store it in self.adc. '''
        data = self.read_all_channels()
        self.adc = np.reshape(data, (2, self.wfm_size))

    def get_spectrum(self):
        fft_adc = np.fft.fft(self.adc, axis=1)
        self.spectrum = fft_adc[:, 0:self.wfm_size / 2]

    def get_avg_spectrum(self, n_avg=1):
        self.avg_spectrum = np.zeros((2, self.wfm_size / 2))
        for i in range(n_avg):
            self.get_adc()
            fft_adc = np.abs(np.fft.fft(self.adc, axis=1))
            self.avg_spectrum += fft_adc[:, 0:self.wfm_size / 2]
        self.avg_spectrum /= n_avg

    # -------------------------------
    # Trigger related functions

    @command()
    def update_now(self):
        ''' This function sends a trigger to update immediately all
        the variables in the FPGA.
        '''
        pass

    @command()
    def always_update(self):
        ''' When this function is called, the FPGA variables do not
        wait for the trigger to be updated.
        '''
        pass

    @command()
    def get_counter(self):
        ''' Return a 64 bits integer that counts the number of clock 
        cycles (8 ns) between the startup of the FPGA and the last trigger
        (beginning of the last acquisition).
        '''
        return self.client.recv_int(8, fmt='Q')

    @command()
    def reset_acquisition(self):
        ''' This function has the same effect as get_adc()
        except it does not return any data
        '''
        pass

    @command(funcname='reset')
    def reset_dac(self):
        pass

    def reset(self):
        self.reset_dac()

    @command()
    def get_first_empty_bram_index(self):
        return self.client.recv_uint32()

