#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib
matplotlib.use('GTKAgg')
from matplotlib import pyplot as plt
import os
import time

from decimator import Decimator
from koheron import connect

host = os.getenv('HOST', '192.168.1.7')
client = connect(host, 'decimator')
driver = Decimator(client)

n = 8192
fs = 125e6 / 512

# Dynamic plot
fig = plt.figure()
ax = fig.add_subplot(111)
x = np.fft.fftshift(np.fft.fftfreq(n) * fs)

y = np.zeros(n)
li, = ax.plot(x, y)
ax.set_ylim((100, 250))
ax.set_xlabel('Frequency (Hz)')
ax.set_ylabel('Power spectral density (dB)')
fig.canvas.draw()

while True:
    try:
        data = driver.read_adc()
        data = np.fft.fftshift(10*np.log10(np.abs(np.fft.fft(data))**2))
        print driver.get_fifo_length()
        li.set_ydata(data)
        fig.canvas.draw()
        plt.pause(0.001)
    except KeyboardInterrupt:
        break
