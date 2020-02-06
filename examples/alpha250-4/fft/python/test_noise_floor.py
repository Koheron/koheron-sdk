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

host = os.getenv('HOST', '192.168.1.50')
client = connect(host, 'fft', restart=False)
driver = FFT(client)

Rload = 50 # Ohm

driver.set_fft_window(0)

driver.set_input_channel(0)
lpsd00 = np.sqrt(Rload * driver.read_psd(0)) # V/rtHz
lpsd10 = np.sqrt(Rload * driver.read_psd(1)) # V/rtHz

driver.set_input_channel(1)
lpsd01 = np.sqrt(Rload * driver.read_psd(0)) # V/rtHz
lpsd11 = np.sqrt(Rload * driver.read_psd(1)) # V/rtHz

fig = plt.figure(figsize=(6,6))
ax = fig.add_subplot(111)
ax.set_xlim([0, 125])
ax.set_ylim([0, 40])

freqs = np.arange(driver.n_pts / 2) * 250. / driver.n_pts

ax.plot(freqs, lpsd00 * 1e9, label='IN0')
ax.plot(freqs, lpsd01 * 1e9, label='IN1')
ax.plot(freqs, lpsd10 * 1e9, label='IN2')
ax.plot(freqs, lpsd11 * 1e9, label='IN3')

ax.set_xlabel('Frequency (MHz)')
ax.set_ylabel('Voltage noise density (nV/rtHz)')

ax.legend()
plt.show()

np.save('alpha250_4_noise_floor.npy', (freqs, lpsd00, lpsd01, lpsd10, lpsd11))