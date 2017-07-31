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

size = int(math.floor((index_high - index_low) / decimation_factor))
t_min = index_low * mhz / sampling_rate
t_max = index_high * mhz / sampling_rate
t = np.linspace(t_min, t_max, size)

oscillo.set_num_average_min(2000)
oscillo.set_average(True)

data = oscillo.get_decimated_data(decimation_factor, index_low, index_high)

data = np.reshape(data, (2, size))

plt.plot(t, data[0,:], 'b')
plt.plot(t, data[1,:], 'g')
plt.axis([t_min, t_max, -8192, 8192])
plt.xlabel('Time (us)')
plt.show()
