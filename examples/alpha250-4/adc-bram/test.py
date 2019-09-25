#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
import time
from adc_bram import AdcBram
from koheron import connect

import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
from matplotlib.lines import Line2D

host = os.getenv('HOST', '192.168.1.50')
client = connect(host, 'adc-bram', restart=False)
driver = AdcBram(client)

print('ADC size = {}'.format(driver.adc_size))

driver.set_reference_clock(0) # External
time.sleep(5)

clk_200MHz = {'idx': 0, 'fs': 200E6}
clk_250MHz = {'idx': 1, 'fs': 250E6}

clock = clk_250MHz
driver.set_sampling_frequency(clock['idx'])
# driver.phase_shift(0)

t = np.arange(driver.adc_size) / clock['fs']
t_us = 1e6 * t

# Dynamic plot
fig = plt.figure()
ax = fig.add_subplot(111)
y = np.zeros(driver.adc_size)
line0 = Line2D([], [], color='blue', label='IN0')
line1 = Line2D([], [], color='green', label='IN1')
line2 = Line2D([], [], color='red', label='IN2')
line3 = Line2D([], [], color='cyan', label='IN3')
ax.add_line(line0)
ax.add_line(line1)
ax.add_line(line2)
ax.add_line(line3)
ax.set_xlabel('Time (us)')
ax.set_ylabel('ADC Raw data')
ax.set_xlim((t_us[0], t_us[-1]))
# ax.set_ylim((-2**15, 2**15))
ax.set_ylim((-300, 300))
ax.legend(loc='upper right')
fig.canvas.draw()

while True:
    try:
        driver.trigger_acquisition()
        time.sleep(0.1)
        driver.get_adc(0)
        driver.get_adc(1)
        line0.set_data(t_us, driver.adc0[0,:])
        line1.set_data(t_us, driver.adc0[1,:])
        line2.set_data(t_us, driver.adc1[0,:])
        line3.set_data(t_us, driver.adc1[1,:])
        fig.canvas.draw()
        plt.pause(0.001)
        # plt.pause(3600)
    except KeyboardInterrupt:
        break