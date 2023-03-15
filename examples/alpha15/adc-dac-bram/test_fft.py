#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
from adc_dac_bram import AdcDacBram
from koheron import connect

from matplotlib import pyplot as plt
import matplotlib as mpl
mpl.style.use('classic')

host = os.getenv('HOST', '192.168.1.113')
client = connect(host, 'adc-dac-bram', restart=False)
driver = AdcDacBram(client)

driver.set_reference_clock(1)

fs = 15E6
n = driver.adc_size
n_avg = 10

# -------------------------------------------------------------------------------
# Set ADC
# -------------------------------------------------------------------------------

input_range = 0

driver.range_select(0, input_range)
driver.range_select(1, input_range)

if input_range == 0:
    input_span = 2.048 # Vpp
else:
    input_span = 8.192 # Vpp

# -------------------------------------------------------------------------------
# Set modulation on DAC
# -------------------------------------------------------------------------------

fs_dac = 240E6
amp_mod = 0.0
ndac = driver.dac_size
t_dac = np.arange(ndac) / fs_dac
f_dac = fs / 50
f_dac = np.round(f_dac / fs_dac * ndac) * fs_dac / ndac
f_mod = f_dac / (fs_dac / fs)
print("fmod = {} kHz".format(f_mod / 1E3))
driver.dac[0, :] = amp_mod * np.sin(2.0 * np.pi * f_dac * t_dac)
driver.dac[1, :] = amp_mod * np.sin(2.0 * np.pi * f_dac * t_dac)
driver.set_dac()

# -------------------------------------------------------------------------------
# Acquire PSD
# -------------------------------------------------------------------------------

f = np.arange((n//2+1)) * fs / n
psd = np.zeros((2,n//2+1))

# win = np.hanning(n)
win = np.ones(n)

driver.get_adc(0)
driver.get_adc(1)

for i in range(n_avg):
    for channel in range(2):
        driver.get_adc(channel)
        data = driver.adc[channel,:]
        noise_lsb_rms = np.std(data)
        data_volts = data * input_span / 2**18
        print("{}: Transition noise = {:.2f} LSBrms, Input noise = {:.2f} uVrms"
              .format(channel, noise_lsb_rms, 1E6 * np.std(data_volts)))
        # plt.plot(data_volts)
        # plt.show()
        psd[channel, :] += 2.0 * np.abs(np.fft.rfft(win * (data_volts - np.mean(data_volts)))) ** 2 / fs / np.sum(win**2)

psd /= n_avg
lpsd = np.sqrt(psd)

ax = plt.subplot(111)
ax.loglog(f, lpsd[0, :] * 1E9, label='ADC0')
ax.loglog(f, lpsd[1, :] * 1E9, label='ADC1')
ax.grid(True, which='major', linestyle='-', linewidth=1.5, color='0.35')
ax.grid(True, which='minor', linestyle='-', color='0.35')
ax.set_axisbelow(True)
ax.set_xlabel("FREQUENCY (Hz)")
ax.set_ylabel(u"VOLTAGE NOISE DENSITY (nV/\u221AHz)")
ax.set_ylim(1., 100.)
plt.legend(loc='best')
plt.show()