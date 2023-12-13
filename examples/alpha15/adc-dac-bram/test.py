#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
import time
from scipy import signal
from adc_dac_bram import AdcDacBram
from koheron import connect

import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
from matplotlib.lines import Line2D

host = os.getenv('HOST', '192.168.1.156')
client = connect(host, 'adc-dac-bram', restart=True)
driver = AdcDacBram(client)

driver.set_reference_clock(0)

input_range = 1

driver.range_select(0, input_range)
driver.range_select(1, input_range)

if input_range == 0:
    input_span = 2.048 # Vpp
else:
    input_span = 8.192 # Vpp

# driver.set_testpat()

print('DAC size = {}'.format(driver.dac_size))
print('ADC size = {}'.format(driver.adc_size))

clk_15MHz = {'idx': 0, 'fs': 15E6}
# driver.set_sampling_frequency(clk_15MHz['idx'])

t_adc = np.arange(driver.dac_size) / clk_15MHz['fs']
t_adc_us = 1e6 * t_adc

# -------------------------------------------------------------------------------
# Set modulation on DAC
# -------------------------------------------------------------------------------

fs_dac = 240E6
amp_mod = 0.99
ndac = driver.dac_size
t_dac = np.arange(ndac) / fs_dac
f_dac = clk_15MHz['fs'] / 200
f_dac = np.round(f_dac / fs_dac * ndac) * fs_dac / ndac
f_mod = f_dac / (fs_dac / clk_15MHz['fs'])
print("fmod = {} kHz".format(f_mod / 1E3))

# driver.dac[0, :] = amp_mod * np.sin(2.0 * np.pi * f_dac * t_dac)
# driver.dac[1, :] = amp_mod * np.sin(2.0 * np.pi * f_dac * t_dac)

driver.dac[0, :] = amp_mod * signal.sawtooth(2.0 * np.pi * f_dac * t_dac, width=0.5)
driver.dac[1, :] = amp_mod * signal.sawtooth(2.0 * np.pi * f_dac * t_dac, width=0.5)

# driver.dac[0, 0:driver.dac_size // 2] = 0.0
# driver.dac[0, driver.dac_size // 2 * 1:-1] = amp_mod
# driver.dac[1, 0:driver.dac_size // 2] = 0.0
# driver.dac[1, driver.dac_size // 2 * 1:-1] = amp_mod

driver.set_dac()

# -------------------------------------------------------------------------------

# Dynamic plot
fig = plt.figure()
ax = fig.add_subplot(111)
y = np.zeros(driver.adc_size)
line0 = Line2D([], [], color='blue', label='ADC0')
line1 = Line2D([], [], color='green', label='ADC1')
ax.add_line(line0)
ax.add_line(line1)
ax.set_xlabel('Time (us)')
ax.set_ylabel('ADC Raw data')
ax.set_xlim((t_adc_us[0], t_adc_us[-1]))
# ax.set_xlim((t_adc_us[0], t_adc_us[0] + 10))
ax.set_ylim((-2**17, 2**17))
# ax.set_ylim((0, 2**19))
# ax.set_ylim((-input_span / 2.0, input_span / 2.0))
ax.legend(loc='upper right')
fig.canvas.draw()

i = 0
nglitch = 0

while True:
    try:
        i += 1

        # driver.dco_delay_tap(0, i % 32)
        # driver.da_delay_tap(0, i % 32)
        # driver.db_delay_tap(0, i % 32)

        # driver.dco_delay_tap(1, i % 32)
        # driver.da_delay_tap(1, i % 32)
        # driver.db_delay_tap(1, i % 32)

        driver.get_adc(0)
        driver.get_adc(1)
        line0.set_data(t_adc_us, driver.adc[0,:])
        line1.set_data(t_adc_us, driver.adc[1,:])
        # line0.set_data(t_adc_us, driver.adc[0,:] * input_span / 2**18)
        # line1.set_data(t_adc_us, driver.adc[1,:] * input_span / 2**18)
        raw_data = driver.adc_raw_data(1)
        print(i, raw_data)
        print("{:018b}, {:018b}".format(raw_data[0], raw_data[1]))
        print("{:05x}, {:05x}".format(raw_data[0], raw_data[1]))
        lsbrms0 = np.std(driver.adc[0,:])
        lsbrms1 = np.std(driver.adc[1,:])
        print("{:.2f}, {:.2f} LSBrms".format(lsbrms0, lsbrms1))

        if lsbrms0 > 3.0 or lsbrms1 > 3.0:
            nglitch += 1
        #     plt.pause(1000)
        print("nglitch / ntot = {} / {} [{:.2f} %]".format(nglitch, i, 100 * nglitch / i))

        fig.canvas.draw()
        plt.pause(0.01)
    except KeyboardInterrupt:
        break
