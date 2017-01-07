#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Function generator with live acquisition plot for RedPitaya

import os
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

from koheron import connect
from oscillo import Oscillo

host = os.getenv('HOST','192.168.1.100')
client = connect(host, name='oscillo')
driver = Oscillo(client)

n = driver.wfm_size # Number of samples
fs = 125E6          # Sampling rate (Hz)
n_bits = 14         # ADC bits

# -----------------------------------------------------------------------------------------------------
# Waveform generation
# -----------------------------------------------------------------------------------------------------

waveform = 'sine'
freq = 1E5 # Hz
amp = 0.2  # V
dac_channel = 0

phi = 2 * np.pi * np.floor(n * freq / fs) / n * np.arange(n)

if waveform == 'triangle':
    driver.dac[dac_channel,:] = amp * signal.sawtooth(phi, width=0.5)
elif waveform == 'sine':
    driver.dac[dac_channel,:] = amp * np.sin(phi)
elif waveform == 'square':
    driver.dac[dac_channel,:] = amp * signal.square(phi)

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
