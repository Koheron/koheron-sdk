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

host = os.getenv('HOST', '192.168.1.43')
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

# Dynamic plot
fig = plt.figure()
ax = fig.add_subplot(111)
y = np.zeros(driver.adc_size)
line0 = Line2D([], [], color='blue', label='ADC0')
line1 = Line2D([], [], color='green', label='ADC1')
ax.add_line(line0)
ax.add_line(line1)
ax.set_xlabel('Time (us)')
ax.set_ylabel('ADC Raw data')
ax.set_xlim((t_us[0], t_us[-1]))
ax.set_ylim((-2**13, 2**13))
ax.legend()
fig.canvas.draw()

i = 0



while True:
    try:
        i = i+1
        print i
        driver.get_adc()
        line0.set_data(t_us, driver.adc[0,:])
        line1.set_data(t_us, driver.adc[1,:])
        fig.canvas.draw()
        plt.pause(0.001)
    except KeyboardInterrupt:
        break
