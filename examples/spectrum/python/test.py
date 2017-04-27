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
index_low = 0
index_high = wfm_size / 2

decimated_data = spectrum.get_decimated_data(decimation_factor, index_low, index_high)

freq_min = 0
freq_max = sampling_rate / mhz / 2

freq_range = np.linspace(freq_min, freq_max, (wfm_size / 2))

plt.plot(freq_range, 10 * np.log10(decimated_data), 'b')
plt.show()
