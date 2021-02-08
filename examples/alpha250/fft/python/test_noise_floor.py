#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
Measure the noise floor of the ADC and ADC + DAC
'''

import numpy as np
import os
import time
import matplotlib.pyplot as plt

from fft import FFT
from koheron import connect

try: input = raw_input
except NameError: pass

host = os.getenv('HOST', '192.168.1.16')
client = connect(host, 'fft', restart=False)
driver = FFT(client)

driver.set_fft_window(0)
driver.set_dds_freq(0, 0)
driver.set_dds_freq(1, 0)
driver.set_input_channel(0)

fig = plt.figure(figsize=(6,6))
ax = fig.add_subplot(111)
ax.set_xlim([0, 125])
ax.set_ylim([0, 40])

freqs = np.arange(driver.n_pts/2) * 250. / 8192

input("Terminate ADC0 with 50 Ohm")
lpsd1 = np.sqrt(50 * driver.read_psd()) # V/rtHz
ax.plot(freqs, lpsd1 * 1e9, label='ADC0 terminated with 50 Ohm')

input("Connect DAC0 to ADC0")
lpsd2 = np.sqrt(50 * driver.read_psd()) # V/rtHz
ax.plot(freqs, lpsd2 * 1e9, label='ADC0 connected to DAC0')

ax.set_xlabel('Frequency (MHz)')
ax.set_ylabel('Voltage noise density (nV/rtHz)')

ax.legend()
plt.show()

np.save('alpha250_noise_floor.npy', (freqs, lpsd1, lpsd2))