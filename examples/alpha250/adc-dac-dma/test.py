#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
from koheron import command, connect
import matplotlib.pyplot as plt
import numpy as np

class AdcDacDma(object):
    def __init__(self, client):
        self.n = 8*1024*1024
        self.client = client
        self.dac = np.zeros((self.n))
        self.adc = np.zeros((self.n))

    @command()
    def select_adc_channel(self, channel):
        pass

    @command()
    def set_dac_data(self, data):
        pass

    def set_dac(self, warning=False, reset=False):
        if warning:
            if np.max(np.abs(self.dac)) >= 1:
                print('WARNING : dac out of bounds')
        dac_data = np.uint32(np.mod(np.floor(32768 * self.dac) + 32768, 65536) + 32768)
        self.set_dac_data(dac_data[::2] + 65536 * dac_data[1::2])

    @command()
    def start_dma(self):
        pass

    @command()
    def stop_dma(self):
        pass

    @command()
    def get_adc_data(self):
        return self.client.recv_array(self.n//2, dtype='uint32')

    def get_adc(self):
        data = self.get_adc_data()
        self.adc[::2] = (np.int32(data % 65536) - 32768) % 65536 - 32768
        self.adc[1::2] = (np.int32(data >> 16) - 32768) % 65536 - 32768

if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.16')
    client = connect(host, name='adc-dac-dma')
    driver = AdcDacDma(client)

    adc_channel = 0
    driver.select_adc_channel(adc_channel)

    fs = 250e6
    fmin = 1e3 # Hz
    fmax = 1e6 # Hz

    t = np.arange(driver.n) / fs
    chirp = (fmax-fmin)/(t[-1]-t[0])

    print("Set DAC waveform (chirp between {} and {} MHz)".format(1e-6*fmin, 1e-6*fmax))
    driver.dac = 0.9 * np.cos(2*np.pi * (fmin + chirp * t) * t)
    driver.set_dac()

    fs = 250e6
    n_avg = 10
    adc = np.zeros(driver.n)

    print("Get ADC{} data ({} points)".format(adc_channel, driver.n))
    driver.start_dma()
    driver.get_adc()
    driver.stop_dma()

    n_pts = 1000000
    print("Plot first {} points".format(n_pts))
    plt.plot(1e6 * t[0:n_pts], driver.adc[0:n_pts])
    plt.ylim((-2**15, 2**15))
    plt.xlabel('Time (us)')
    plt.ylabel('ADC Raw data')
    plt.show()