#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
import time
import matplotlib.pyplot as plt

from fft import FFT
from koheron import connect

host = os.getenv('HOST', '192.168.1.11')
client = connect(host, 'fft', restart=False)
driver = FFT(client)

print('Start test.py')

n_pts = driver.n_pts
fs = 250e6

psd = driver.read_psd()

#plt.plot(10*np.log10(psd))
plt.plot(psd)
plt.show()

#freqs = np.array([0.01, 0.02, 0.05, 0.1, 0.15, 0.2, 0.25, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1, 1.5, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 22, 25, 27, 30, 33, 35, 37, 40, 43, 45, 47, 50, 53, 55, 57, 60, 63, 65, 67, 70, 73, 75, 77, 80, 83, 85, 87, 90, 93, 95, 97, 100, 103, 105, 107, 110, 113, 115, 117, 120, 123, 124]) * 1e6

freqs = np.linspace(0.01e6, 40e6,num=200)
freqs = np.round(freqs / fs * n_pts) * fs / n_pts

hd1 = 0.0 * freqs
hd2 = 0.0 * freqs
hd3 = 0.0 * freqs

snr = 0.0 * freqs

for i, freq in enumerate(freqs):
    n = np.uint32(freq / fs * n_pts)
    driver.set_dds_freq(0, freq)
    time.sleep(0.5)
    psd = driver.read_psd()
    psd_db = 10*np.log10(psd)

    snr[i] = 10*np.log10(psd[n-1] / (np.sum(psd) - psd[n-1]))

    hd1[i] = psd_db[n-1]

    if 2*(n-1) < n_pts/2:
        hd2[i] = psd_db[2*n-1] - hd1[i]
    else:
        hd2[i] = psd_db[n_pts - (2*n) - 1] - hd1[i]

    if 3*(n-1) < n_pts/2:
        hd3[i] = psd_db[3*n-1] - hd1[i]
    elif 3*(n-1) < n_pts:
        hd3[i] = psd_db[n_pts - (3*n) - 1] - hd1[i]
    else:
        hd3[i] = psd_db[(3*n)%(n_pts/2) - 1] - hd1[i]

    print i, freq, hd1[i], hd2[i], hd3[i]

plt.xlabel('Frequency (MHz)')
plt.ylabel('Harmonic distortion (dB)')
plt.semilogx(freqs*1e-6, hd2, label='HD2')
plt.semilogx(freqs*1e-6, hd3, label='HD3')
plt.legend()

plt.show()

plt.xlabel('Frequency (MHz)')
plt.ylabel('SNR (dB)')
plt.plot(freqs*1e-6, snr)
plt.show()

plt.xlabel('Frequency (MHz)')
plt.ylabel('Response (dB)')
plt.plot(freqs*1e-6, hd1)
plt.show()
