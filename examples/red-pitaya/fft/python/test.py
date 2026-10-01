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
fs = driver.get_fs()

psd = driver.read_psd()

plt.plot(psd)
plt.show()

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

    snr[i] = 10*np.log10(psd[n] / (np.sum(psd) - psd[n]))

    hd1[i] = psd_db[n]

    # Fold harmonics into the one-sided spectrum. Nyquist is not returned.
    for harmonic, values in ((2, hd2), (3, hd3)):
        bin_index = (harmonic * int(n)) % n_pts
        bin_index = min(bin_index, n_pts - bin_index)
        values[i] = (psd_db[bin_index] - hd1[i]
                     if bin_index < len(psd_db) else np.nan)

    print(i, freq, hd1[i], hd2[i], hd3[i])

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
