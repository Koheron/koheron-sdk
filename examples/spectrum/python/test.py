#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import connect
from spectrum import Spectrum

import os
import numpy as np
import math

import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt

host = os.getenv('HOST','192.168.1.100')
client = connect(host, name='spectrum')

spectrum = Spectrum(client)
spectrum.reset_acquisition()

wfm_size = 4096
mhz = 1e6
sampling_rate = 125e6

spectrum.reset_acquisition()

decimation_factor = 1
index_low = 1
index_high = wfm_size / 2

decimated_data = spectrum.get_decimated_data(decimation_factor, index_low, index_high)

print(decimated_data)

freq_min = 0
freq_max = sampling_rate / mhz / 2

freq_range = np.linspace(freq_min, freq_max, wfm_size)

plot_data = np.zeros((wfm_size, 2))

for i in range(0, len(decimated_data) - 1):
    freq = (index_low + i) * sampling_rate / wfm_size / mhz
    psd = 10 * math.log10(decimated_data[i])
    plot_data[i] = [freq, psd]

print(plot_data)

plt.plot(freq_range, plot_data, 'b')
plt.axis([0, 62.5, 125, 175])
plt.show()
