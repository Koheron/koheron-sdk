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
from koheron import Alpha250

host = os.getenv('HOST', '192.168.1.50')
client = connect(host, 'fft', restart=False)
driver = FFT(client)
alpha = Alpha250(client)

print(driver.get_fs())

alpha.set_precision_dac(3, 16384)
alpha.set_precision_dac(1, 49152)
alpha.set_precision_dac(2, 32768)
alpha.set_precision_dac(0, 65500)

driver.set_fft_window(1)
driver.set_input_channel(0)

freqs = np.arange(driver.n_pts / 2) * 250. / driver.n_pts
plt.loglog(freqs, driver.read_psd_raw(0))
plt.loglog(freqs, driver.read_psd_raw(1))
plt.show()
