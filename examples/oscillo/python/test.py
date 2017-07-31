#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import connect
from oscillo import Oscillo

import time

import os
import numpy as np
import math

import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt

host = os.getenv('HOST','192.168.1.100')
client = connect(host, name='oscillo')

driver = Oscillo(client)
driver.reset()

decimation_factor = 1
index_low = 0
index_high = 8192

size = int(math.floor((index_high - index_low) / decimation_factor))
t_min = 1e6*index_low / driver.sampling_rate # µs
t_max = 1e6*index_high / driver.sampling_rate # µs
t = np.linspace(t_min, t_max, size)

# Modulation on DAC
amp_mod = 0.2
freq_mod = driver.sampling_rate / driver.wfm_size * 10
driver.dac[0, :] = amp_mod*np.cos(2 * np.pi * freq_mod * driver.t)
driver.dac[1, :] = amp_mod*np.sin(2 * np.pi * freq_mod * driver.t)
driver.set_dac()

time.sleep(0.001)

driver.set_num_average_min(2000)
driver.set_average(True)

data = driver.get_decimated_data(decimation_factor, index_low, index_high)

print size

data = np.reshape(data, (2, size))

plt.plot(t, data[0,:], 'b')
plt.plot(t, data[1,:], 'g')
plt.axis([t_min, t_max, -8192, 8192])
plt.xlabel('Time (us)')
plt.show()
