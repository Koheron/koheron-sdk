#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import math

from sampling import Sampling
from koheron_tcp_client import command, write_buffer

class Base(object):
    """ This class is used as a base class for `Oscillo` and `Spectrum`

    args:
        wfm_size: number of points in the waveform.
        client : instance of KClient class, used to connect to the board.
    """

    def __init__(self, wfm_size, client):
        self.client = client
        self.open_base(wfm_size)

        self.n = wfm_size
        self.max_current = 40  # mA
        self.sampling = Sampling(wfm_size, 125e6)

        self.opened = True
        self.dac = np.zeros((2, self.sampling.n))

        self.failed = False

    def open_base(self, wfm_size):
        @command('BASE')
        def open(self, wfm_size):
            pass

        open(self, wfm_size)

    def update(self):
        pass  # Used in BaseSimu

    def close(self):
        self.reset()

    @command('BASE')
    def reset(self):
        pass

    @command('BASE')
    def start_laser(self):
        pass

    @command('BASE')
    def stop_laser(self):
        pass

    @command('BASE')
    def get_laser_current(self):
        current = self.client.recv_int(4)

        if math.isnan(current):
            print("Can't read laser current")
            self.failed = True

        return (0.0001/21.) * current

    @command('BASE')
    def get_laser_power(self):
        power = self.client.recv_int(4)

        if math.isnan(power):
            print("Can't read laser power")
            self.failed = True

        return power

    @command('BASE')
    def get_monitoring(self):
        return self.client.recv_tuple()

    @command('BASE')
    def set_laser_current(self, current):
        """ current: The bias in mA """
        pass

    @write_buffer('BASE')
    def set_dac_buffer(self, data):
        pass

    def set_dac(self, warning=False, reset=False):
        if warning:
            if np.max(np.abs(self.dac)) >= 1:
                print('WARNING : dac out of bounds')
        dac_data_1 = np.mod(np.floor(8192 * self.dac[0, :]) + 8192,16384) + 8192
        dac_data_2 = np.mod(np.floor(8192 * self.dac[1, :]) + 8192,16384) + 8192
        self.set_dac_buffer(dac_data_1 + 65536 * dac_data_2)

        if reset:
            self.reset_acquisition()

    @command('BASE')
    def get_bitstream_id(self):
        return self.client.recv_buffer(8, data_type='int32')

    @command('BASE')
    def set_led(self, value):
        pass

    @command('BASE')
    def reset_acquisition(self):
        pass
