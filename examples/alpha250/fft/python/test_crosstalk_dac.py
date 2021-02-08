#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
Measure the crosstalk between DAC0 and DAC1
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

# Use Rectangular FFT window
driver.set_fft_window(0)

n_pts = driver.n_pts
fs = 250e6

fmin = 0.1e6
fmax = 120e6

freqs = np.linspace(fmin, fmax ,num=100)
freqs = np.round(freqs / fs * n_pts) * fs / n_pts

crosstalk = np.zeros((2, np.size(freqs)))

fig = plt.figure(figsize=(6,6))
ax = fig.add_subplot(111)

ax.set_xlabel('Frequency (MHz)')
ax.set_ylabel('Crosstalk (dB)')

input("Connect DAC0 to ADC0")
input("Connect DAC1 to ADC1")

driver.set_dds_freq(1, 0)

for i, freq in enumerate(freqs):
    n = np.uint32(freq / fs * n_pts)
    driver.set_dds_freq(0, freq)

    driver.set_input_channel(0)
    time.sleep(0.5)
    psd0 = driver.read_psd()

    driver.set_input_channel(1)
    time.sleep(0.5)
    psd1 = driver.read_psd()

    crosstalk[0, i] = 10 * np.log10(psd1[n-1] / psd0[n-1])

    print(freq, crosstalk[0, i])

driver.set_dds_freq(0, 0)

for i, freq in enumerate(freqs):
    n = np.uint32(freq / fs * n_pts)
    driver.set_dds_freq(1, freq)

    driver.set_input_channel(0)
    time.sleep(0.5)
    psd0 = driver.read_psd()

    driver.set_input_channel(1)
    time.sleep(0.5)
    psd1 = driver.read_psd()

    crosstalk[1, i] = 10 * np.log10(psd0[n-1] / psd1[n-1])

    print(freq, crosstalk[1, i])

ax.plot(freqs*1e-6, crosstalk[0], label="DAC0 to DAC1")
ax.plot(freqs*1e-6, crosstalk[1], label="DAC1 to DAC0")
ax.set_xlim([fmin * 1e-6, fmax * 1e-6])
ax.legend(loc=2)

plt.show()

np.save('alpha250_dac_crosstalk.npy', (freqs, crosstalk))