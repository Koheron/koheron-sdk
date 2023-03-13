#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
import time
from adc_dac_bram import AdcDacBram
from koheron import connect

from matplotlib import pyplot as plt
import matplotlib as mpl
mpl.style.use('classic')

host = os.getenv('HOST', '192.168.1.113')
client = connect(host, 'adc-dac-bram', restart=True)
driver = AdcDacBram(client)

input_range = 1
channel = 0

driver.range_select(channel, input_range)

if input_range == 0:
    input_span = 2.048 # Vpp
else:
    input_span = 8.192 # Vpp

driver.get_adc(channel)
data = driver.adc[channel,:]

plt.plot(data)
plt.show()

data_volts = data * input_span / 2**18

print("Transition noise = {} LSBrms".format(np.std(data)))
print("Input noise = {} uVrms".format(1E6 * np.std(data_volts)))

ax = plt.subplot(111)
ax.hist(data, bins='auto')
ax.grid(True, which='major', linestyle='-', linewidth=1.5, color='0.35')
ax.grid(True, which='minor', linestyle='-', color='0.35')
ax.set_axisbelow(True)
ax.set_xlabel("OUTPUT CODE (LSBs)")
ax.set_ylabel("COUNT")
plt.show()