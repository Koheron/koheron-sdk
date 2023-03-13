#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
import time
from adc_dac_bram import AdcDacBram
from koheron import connect

import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
from matplotlib.lines import Line2D

host = os.getenv('HOST', '192.168.1.113')
client = connect(host, 'alpha15-adc-dac-bram', restart=True)
driver = AdcDacBram(client)

driver.range_select(0, 0)
driver.range_select(1, 0)

# driver.set_testpat()

print('DAC size = {}'.format(driver.dac_size))
print('ADC size = {}'.format(driver.adc_size))

clk_15MHz = {'idx': 0, 'fs': 15E6}
# driver.set_sampling_frequency(clk_15MHz['idx'])

t_adc = np.arange(driver.dac_size) / clk_15MHz['fs']
t_adc_us = 1e6 * t_adc

driver.set_testpat()

# -------------------------------------------------------------------------------
# Set modulation on DAC
# -------------------------------------------------------------------------------

fs_dac = 240E6
amp_mod = 0.99
ndac = driver.dac_size
t_dac = np.arange(ndac) / fs_dac
f_dac = clk_15MHz['fs'] / 50
f_dac = np.round(f_dac / fs_dac * ndac) * fs_dac / ndac
f_mod = f_dac / (fs_dac / clk_15MHz['fs'])
print("fmod = {} kHz".format(f_mod / 1E3))
driver.dac[0, :] = amp_mod * np.sin(2.0 * np.pi * f_dac * t_dac)
driver.dac[1, :] = amp_mod * np.sin(2.0 * np.pi * f_dac * t_dac)
# driver.dac[0, 0:driver.dac_size // 2] = 0.0
# driver.dac[0, driver.dac_size // 2 * 1:-1] = amp_mod
# driver.dac[1, 0:driver.dac_size // 2] = 0.0
# driver.dac[1, driver.dac_size // 2 * 1:-1] = amp_mod
driver.set_dac()

# -------------------------------------------------------------------------------

n = 32
data = np.zeros((2, n))

for i in range (n):
    driver.clkout_dec()
    driver.get_adc(0)
    driver.get_adc(1)
    raw_data = driver.adc_raw_data()
    print(i, i % 16, raw_data)
    print("{0:018b}, {0:018b}".format(raw_data[0], raw_data[1]))

    # data[0,i] = np.std(np.diff(driver.adc[0,:]))
    # data[1,i] = np.std(np.diff(driver.adc[1,:]))

    data[0,i] = np.mean(driver.adc[0,:])
    data[1,i] = np.mean(driver.adc[1,:])

    time.sleep(0.1)

plt.plot(data[0,:], label='ADC0')
plt.plot(data[1,:], label='ADC1')
plt.legend(loc='upper right')
plt.show()