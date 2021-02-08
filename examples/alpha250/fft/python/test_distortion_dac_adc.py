#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
Measure the harmonic distortion (HD2 and HD3) of the DAC and ADC combined.
Connect DAC0 to ADC0 to perform the measurement.
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

# Use Rectangular FFT window
driver.set_fft_window(0)

n_pts = driver.n_pts
fs = 250e6

fmin = 0.1e6
fmax = 40e6

freqs = np.linspace(fmin, fmax ,num=500)
freqs = np.round(freqs / fs * n_pts) * fs / n_pts

hd1 = 0.0 * freqs
hd2 = 0.0 * freqs
hd3 = 0.0 * freqs

for i, freq in enumerate(freqs):
    n = np.uint32(freq / fs * n_pts)
    driver.set_dds_freq(0, freq)
    time.sleep(0.5)
    psd = driver.read_psd()
    psd_db = 10*np.log10(psd)

    hd1[i] = psd_db[n-1]
    hd2[i] = psd_db[2*n-1] - hd1[i]
    hd3[i] = psd_db[3*n-1] - hd1[i]

    print(freq, hd1[i], hd2[i], hd3[i])

fig = plt.figure(figsize=(6,6))
ax = fig.add_subplot(111)

ax.set_xlabel('Frequency (MHz)')
ax.set_ylabel('Harmonic distortion (dB)')
ax.semilogx(freqs*1e-6, hd2, label='HD2')
ax.semilogx(freqs*1e-6, hd3, label='HD3')
ax.set_xlim([fmin * 1e-6, fmax * 1e-6])
ax.legend(loc=2)

plt.show()

np.save('alpha250_dac_distortion.npy', (freqs, hd2, hd3))