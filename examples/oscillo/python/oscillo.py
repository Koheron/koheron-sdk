#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from koheron import command

class Oscillo(object):
    def __init__(self, client):
        self.client = client

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

    @command()
    def reset_acquisition(self):
        pass

    @command(funcname='reset')
    def reset_dac(self):
        pass

    def reset(self):
        self.reset_dac()

    # Modulation

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