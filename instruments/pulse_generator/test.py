#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib
matplotlib.use('GTKAgg')
from matplotlib import pyplot as plt
import os
import time

from pulse import Pulse
from koheron import connect

host = os.getenv('HOST', '192.168.1.7')
client = connect(host, name='pulse_generator')
driver = Pulse(client)

pulse_width = 128
n_pulse = 64
pulse_frequency = 1000

pulse_period = np.uint32(driver.fs / pulse_frequency)

# Send Gaussian pulses to DACs
t = np.arange(driver.n_pts) / driver.fs # time grid (s)
driver.dac[0,:] = 0.6 * np.exp(-(t - 500e-9)**2/(150e-9)**2)
driver.dac[1,:] = 0.6 * np.exp(-(t - 500e-9)**2/(150e-9)**2)
driver.set_dac()

driver.set_pulse_generator(pulse_width, pulse_period)

n = pulse_width * n_pulse

# Dynamic plot
fig = plt.figure()
ax = fig.add_subplot(111)
x = np.arange(n)
y = np.zeros(n)
li, = ax.plot(x, y)
ax.set_ylim((-200, 5200))
ax.set_xlabel('FIFO sample number')
ax.set_ylabel('ADC raw value')
fig.canvas.draw()

while True:
    try:
        data_rcv = driver.get_next_pulse(n)
        adc0 = (np.int32(data_rcv % 16384) - 8192) % 16384 - 8192
        adc1 = (np.int32((data_rcv >> 16) % 16384) - 8192) % 16384 - 8192
        print driver.get_fifo_length(), np.mean(adc0), np.mean(adc1)
        li.set_ydata(adc0)
        fig.canvas.draw()
        plt.pause(0.001)
    except KeyboardInterrupt:
        break
