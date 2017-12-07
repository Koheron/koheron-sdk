#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
import time
import matplotlib.pyplot as plt

from fft import FFT
from koheron import connect

host = os.getenv('HOST', '192.168.1.10')
client = connect(host, 'fft', restart=False)
driver = FFT(client)

n_pts = driver.n_pts
fs = driver.fs

freqs = np.linspace(0.1e6, 10e6,num=100)
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
