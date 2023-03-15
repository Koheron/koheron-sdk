#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
from koheron import command, connect
import matplotlib.pyplot as plt
import matplotlib as mpl
mpl.style.use('classic')
import numpy as np
from adc_dac_dma import AdcDacDma

host = os.getenv('HOST','192.168.1.113')
client = connect(host, name='adc-dac-dma')
driver = AdcDacDma(client)

n_avg = 5

# -------------------------------------------------------------------------------
# Set ADC
# -------------------------------------------------------------------------------

input_range = 1

driver.range_select(0, input_range)
driver.range_select(1, input_range)

if input_range == 0:
    input_span = 2.048 # Vpp
else:
    input_span = 8.192 # Vpp

adc_channel = 0
driver.select_adc_channel(adc_channel)

# -------------------------------------------------------------------------------
# Run DMA
# -------------------------------------------------------------------------------

fs_adc = 15E6
adc = np.zeros(driver.n)

print("Get ADC{} data ({} points)".format(adc_channel, driver.n))
driver.start_dma()
driver.get_adc()
driver.stop_dma()

n_pts = driver.n // 2
t_adc = np.arange(n_pts) / fs_adc
print("Plot first {} points".format(n_pts))
plt.plot(1E6 * t_adc, driver.adc)
plt.ylim((-2**17, 2**17))
plt.xlabel('Time (us)')
plt.ylabel('ADC Raw data')
plt.show()

# -------------------------------------------------------------------------------
# Acquire PSD
# -------------------------------------------------------------------------------

f = np.arange((n_pts // 2 + 1)) * fs_adc / n_pts
psd = np.zeros((n_pts // 2 + 1))

# win = np.hanning(n)
win = np.ones(n_pts)

for i in range(n_avg):
    driver.start_dma()
    driver.get_adc()
    driver.stop_dma()
    data = driver.adc
    noise_lsb_rms = np.std(data)
    data_volts = data * input_span / 2**18
    print("Transition noise = {:.2f} LSBrms, Input noise = {:.2f} uVrms"
            .format( noise_lsb_rms, 1E6 * np.std(data_volts)))
    # plt.plot(data_volts)
    # plt.show()
    psd += 2.0 * np.abs(np.fft.rfft(win * (data_volts - np.mean(data_volts)))) ** 2 / fs_adc / np.sum(win**2)

psd /= n_avg
lpsd = np.sqrt(psd)

ax = plt.subplot(111)
ax.loglog(f, lpsd * 1E9, label='ADC{}'.format(adc_channel))
ax.grid(True, which='major', linestyle='-', linewidth=1.5, color='0.35')
ax.grid(True, which='minor', linestyle='-', color='0.35')
ax.set_axisbelow(True)
ax.set_xlabel("FREQUENCY (Hz)")
ax.set_ylabel(u"VOLTAGE NOISE DENSITY (nV/\u221AHz)")
# ax.set_ylim(1., 100.)
plt.legend(loc='best')
plt.show()