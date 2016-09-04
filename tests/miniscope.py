#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Function generator with live acquisition plot

import context
import os
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

from koheron import load_instrument
from drivers.oscillo import Oscillo

host = os.getenv('HOST','192.168.1.100')
client = load_instrument(host, instrument='oscillo')
driver = Oscillo(client)

n = driver.wfm_size # Number of samples
fs = 125E6          # Sampling rate
n_bits = 14

# -----------------------------------------------------------------------------------------------------
# Waveform generation
# -----------------------------------------------------------------------------------------------------

freq = 10
mod_amp = 0.2
waveform = 'sine'
dac_channel = 0

if waveform == 'triangle':
    driver.dac[dac_channel,:] = mod_amp * signal.sawtooth(2 * np.pi * freq / n * np.arange(n), width=0.5)
elif waveform == 'sine':
    driver.dac[dac_channel,:] = mod_amp * np.sin(2 * np.pi * freq / n * np.arange(n))
elif waveform == 'square':
    driver.dac[dac_channel,:] = mod_amp * signal.square(2 * np.pi * freq / n * np.arange(n))

driver.set_dac()

# -----------------------------------------------------------------------------------------------------
# Data acquisition
# -----------------------------------------------------------------------------------------------------

adc_channel = 0

driver.set_averaging(True)

fig = plt.figure()
ax = fig.add_subplot(111)
t = 1E6 * np.arange(n) / fs # Time axis in us
y = np.zeros(n)
li, = ax.plot(t, y)
ax.set_xlabel('Time (us)')
ax.set_ylabel('Signal (V)')
fig.canvas.draw()
plt.show(block=False)

while True:
    try:
        driver.get_adc()
        li.set_ydata(driver.adc[adc_channel,:] / 2**n_bits)
        ax.relim()
        ax.autoscale_view(True, True, True)
        fig.canvas.draw()

    except KeyboardInterrupt:
        break
