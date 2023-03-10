#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
from koheron import command, connect
import matplotlib.pyplot as plt
import numpy as np

class AdcDma(object):
    def __init__(self, client):
        self.n = 8*1024*1024
        self.client = client
        self.adc = np.zeros((self.n))

    @command()
    def select_adc_channel(self, channel):
        pass

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

    @command(classname='ClockGenerator')
    def set_sampling_frequency(self, val):
        pass

if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.16')
    client = connect(host, name='adc-dma')
    driver = AdcDma(client)

    adc_channel = 0
    driver.select_adc_channel(adc_channel)

    fs = 250e6
    t = np.arange(driver.n) / fs
    adc = np.zeros(driver.n)

    driver.set_sampling_frequency(0)

    print("Get ADC{} data ({} points)".format(adc_channel, driver.n))
    driver.start_dma()
    driver.get_adc()
    driver.stop_dma()

    n_pts = 1000
    print("Plot first {} points".format(n_pts))
    plt.plot(1e6 * t[0:n_pts], driver.adc[0:n_pts])
    plt.ylim((-2**15, 2**15))
    plt.xlabel('Time (us)')
    plt.ylabel('ADC Raw data')
    plt.show()