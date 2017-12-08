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

host = os.getenv('HOST', '192.168.1.16')
client = connect(host, 'fft', restart=False)
driver = FFT(client)

driver.set_fft_window(0)
driver.set_dds_freq(0, 0)

fig = plt.figure(figsize=(6,6))
ax = fig.add_subplot(111)
ax.set_xlim([0, 125])
ax.set_ylim([0, 40])

freqs = np.arange(driver.n_pts/2) * 250. / 8192

raw_input("Terminate ADC0 with 50 Ohm")
psd = driver.read_psd()
ax.plot(freqs, np.sqrt(50 * psd) * 1e9, label='ADC0 terminated with 50 Ohm')

raw_input("Connect DAC0 to ADC0")
psd = driver.read_psd()
ax.plot(freqs, np.sqrt(50 * psd) * 1e9, label='ADC0 connected to DAC0')

ax.set_xlabel('Frequency (MHz)')
ax.set_ylabel('Voltage noise density (nV/rtHz)')

ax.legend()

plt.show()
