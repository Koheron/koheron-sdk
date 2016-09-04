#!/usr/bin/env python
# -*- coding: utf-8 -*-

import context
import os
import time
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

from koheron import load_instrument
from drivers.oscillo import Oscillo

host = os.getenv('HOST','192.168.1.100')
client = load_instrument(host, instrument='oscillo')
driver = Oscillo(client)

n = driver.wfm_size

freq = 1
mod_amp = 0.2

# Modulate with a triangle waveform of period 8192 x 8 ns
driver.dac[0,:] = mod_amp * signal.sawtooth(2 * np.pi * freq / n * np.arange(n), width=0.5)
driver.dac[1,:] = mod_amp * signal.sawtooth(2 * np.pi * freq / n * np.arange(n), width=0.5)
driver.set_dac()

driver.set_averaging(True)
time.sleep(0.1)


fig = plt.figure()
ax = fig.add_subplot(111)
x = np.arange(n)
y = np.ones(n)
li, = ax.plot(x, y)
fig.canvas.draw()
plt.show(block=False)

while True:
    try:
        driver.get_adc()
        y = driver.adc[0,:]

        li.set_ydata(y)
        ax.relim()
        ax.autoscale_view(True,True,True)
        fig.canvas.draw()
        # plt.plot(x, y)
        # plt.draw()

    except KeyboardInterrupt:
        break
