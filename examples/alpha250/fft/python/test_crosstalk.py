#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
Measure the crosstalk between ADC0 and ADC1
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
fmax = 120e6

freqs = np.linspace(fmin, fmax ,num=100)
freqs = np.round(freqs / fs * n_pts) * fs / n_pts

crosstalk = np.zeros((2, np.size(freqs)))

fig = plt.figure(figsize=(6,6))
ax = fig.add_subplot(111)

ax.set_xlabel('Frequency (MHz)')
ax.set_ylabel('Crosstalk (dB)')

for j in range(2):

	raw_input("Connect DAC0 to ADC"+str(j))

	for i, freq in enumerate(freqs):
	    n = np.uint32(freq / fs * n_pts)
	    driver.set_dds_freq(0, freq)

	    driver.set_input_channel(j%2)
	    time.sleep(0.5)
	    psd0 = driver.read_psd()

	    driver.set_input_channel((j+1)%2)
	    time.sleep(0.5)
	    psd1 = driver.read_psd()

	    crosstalk[j,i] = 10 * np.log10(psd1[n-1] / psd0[n-1])

	    print freq, crosstalk[j,i]

	ax.plot(freqs*1e-6, crosstalk[j,:], label='ADC{} to ADC{}'.format(j%2, (j+1)%2))



ax.set_xlim([fmin * 1e-6, fmax * 1e-6])
ax.legend(loc=2)

plt.show()
