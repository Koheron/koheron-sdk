#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
from koheron import command, connect
import matplotlib.pyplot as plt
import numpy as np

class SignalAnalyzer(object):
    def __init__(self, client):
        self.client = client

    def _to_two_complement(self, data):
        nbits = 18
        nmax = 2**nbits
        return (np.int32(data % nmax) - nmax/2) % nmax - nmax/2

    @command(classname='Dma')
    def get_data(self):
        return self._to_two_complement(self.client.recv_array(1000000, dtype='int32'))

    @command(classname='FFT')
    def select_adc_channel(self, channel):
        pass

    @command(classname='Ltc2387')
    def range_select(self, channel, range):
        pass

if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.122')
    client = connect(host, name='signal-analyzer')
    driver = SignalAnalyzer(client)

    for channel in [0,1]:
        for range in [0,1]:

            data = driver.select_adc_channel(channel)
            data = driver.range_select(channel,range)
            data = driver.get_data()
            data = driver.get_data()

            print(f"CHANNEL = {channel} / RANGE = {[2,8][range]} V")
            print(f"Mean      = {np.mean(data)}")
            print(f"Std. dev. = {np.std(data)}")
            print(f"Min       = {np.min(data)}")
            print(f"Max       = {np.max(data)}")
            print()

