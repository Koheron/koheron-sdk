#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import connect
from oscillo import Oscillo

import os
import numpy as np
import math

import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt

host = os.getenv('HOST','192.168.1.100')
client = connect(host, name='oscillo')

oscillo = Oscillo(client)
oscillo.reset()

wfm_size = 8192
mhz = 1e6
sampling_rate = 125e6

decimation_factor = 1
index_low = 0
index_high = 8191

decimated_data = oscillo.get_decimated_data(decimation_factor, index_low, index_high)

t_min = index_low * mhz / sampling_rate
t_max = index_high * mhz / sampling_rate

t_range = np.linspace(t_min, t_max, wfm_size)

channel0 = np.zeros((wfm_size, 2))
channel1 = np.zeros((wfm_size, 2))

coeff = mhz * decimation_factor / sampling_rate
size = int(math.floor((index_high - index_low) / decimation_factor))

for i in range(0, wfm_size - 1):
    channel0[i] = [t_min + coeff * i, decimated_data[i]]
    channel1[i] = [t_min + coeff * i, decimated_data[i + size]]

oscillo.set_num_average_min(2000)
oscillo.set_average(True)

plt.plot(t_range, channel0, 'b')
plt.plot(t_range, channel1, 'g')
plt.axis([50, 50.30, -1500, 1500])
plt.show()
