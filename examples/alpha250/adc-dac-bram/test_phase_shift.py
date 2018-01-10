#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
import time
from adc_dac_bram import AdcDacBram
from koheron import connect

import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
from matplotlib.lines import Line2D

host = os.getenv('HOST', '192.168.1.16')
client = connect(host, 'adc-dac-bram', restart=False)
driver = AdcDacBram(client)

print('DAC size = {}'.format(driver.dac_size))
print('ADC size = {}'.format(driver.adc_size))

sampling_frequency = 250e6 # Hz
t = np.arange(driver.dac_size) / sampling_frequency
t_us = 1e6 * t

# Set modulation on DAC
amp_mod = 0.99
freq_mod = sampling_frequency / driver.dac_size * 10
driver.dac[0, :] = amp_mod * np.cos(2 * np.pi * freq_mod * t)
driver.dac[1, :] = amp_mod * np.sin(2 * np.pi * freq_mod * t)
driver.set_dac()

n = 400
data = np.zeros((2,n))

for i in range(n):
    print i
    driver.phase_shift(1)
    driver.get_adc()
    data[0,i] = np.std(np.diff(driver.adc[0]))
    data[1,i] = np.std(np.diff(driver.adc[1]))

plt.plot(data[0,:])
plt.plot(data[1,:])
plt.show()