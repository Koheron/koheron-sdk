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

# Modulation on DAC
amp_mod = 0.2
freq_mod = driver.sampling_rate / driver.wfm_size * 10
driver.dac[0, :] = amp_mod*np.cos(2 * np.pi * freq_mod * driver.t)
driver.dac[1, :] = amp_mod*np.sin(2 * np.pi * freq_mod * driver.t)
driver.set_dac()

time.sleep(0.001)

driver.set_num_average_min(2000)
driver.set_average(True)

driver.get_adc()

plt.plot(1e6*driver.t, driver.adc[0,:], 'b')
plt.plot(1e6*driver.t, driver.adc[1,:], 'g')
plt.axis([0, 1e6*np.max(driver.t), -8192, 8192])
plt.xlabel('Time (us)')
plt.show()
