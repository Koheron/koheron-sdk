#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
import os
import time

from neutronpulse import NeutronPulse
from koheron import connect

host = os.getenv('HOST', '10.210.1.44')
client = connect(host, name='neutron_pulse_generator')
driver = NeutronPulse(client)

# pulse_width = 128
# n_pulse = 64
# pulse_frequency = 1000

#pulse_period = np.uint32(driver.fs / pulse_frequency)

# Send Gaussian pulses to DACs
t = np.arange(driver.n_pts) / driver.fs # time grid (s)
driver.dac[0,:] = 0.6 * np.exp(-(t - 500e-9)**2/(150e-9)**2)
driver.dac[1,:] = 0.6 * np.exp(-(t - 500e-9)**2/(150e-9)**2)
driver.set_dac()

# driver.set_pulse_generator(pulse_width, pulse_period)

# n = pulse_width * n_pulse
n=8000

# Dynamic plot
fig = plt.figure()
ax = fig.add_subplot(111)
x = np.arange(n)
y = np.zeros(n)
li, = ax.plot(x, y)
ax.set_ylim((0, 20000))
ax.set_xlabel('FIFO sample number')
ax.set_ylabel('ADC raw value')
fig.canvas.draw()
print('enterring loop')
while True:
    try:
        data_rcv = driver.get_fifo_data(n)
        adc0 = (np.int32(data_rcv % (2**16)) )  #16384) - 8192) % 16384 - 8192
        adc1 = (np.int32((data_rcv//(2**16)) % 2**16) )  #16384) - 8192) % 16384 - 8192
        print(driver.get_fifo_length(), np.mean(adc0), np.mean(adc1))
        li.set_ydata(adc1)
        fig.canvas.draw()
        plt.pause(0.001)
    except KeyboardInterrupt:
        break
