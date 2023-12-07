#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
import os
import time

from decimator import Decimator
from fft import FFT
from koheron import connect

host = os.getenv('HOST', '192.168.1.113')
client = connect(host, 'signal-analyzer')
decimator = Decimator(client)
fft = FFT(client)

# n = 8192
# m = 16
# data = np.zeros(n*m)

# decimator.read_adc()
# decimator.read_adc()

# for i in range(m):
#     print(i)
#     data[8192*i:8192*i+8192] = driver.read_adc()

# plt.plot(data)
# plt.show()


fft.set_fft_window(0)
fft.set_input_channel(0)

fig = plt.figure(figsize=(6,6))
ax = fig.add_subplot(111)
ax.set_xlim([0, 125])
ax.set_ylim([0, 40])

freqs = np.arange(driver.n_pts/2) * 15. / 8192

psd = np.sqrt(driver.read_psd()) # V/rtHz
ax.plot(freqs, psd * 1e9, label='psd')

ax.set_xlabel('Frequency (MHz)')
ax.set_ylabel('Voltage noise density (U.A.)')

ax.legend()
plt.show()