#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
import os
import time

from decimator import Decimator
from fft import FFT
from koheron import connect

host = os.getenv('HOST', '192.168.1.129')
client = connect(host, 'signal-analyzer')
decimator = Decimator(client)
fft = FFT(client)


fft.select_adc_channel(0)
fft.range_select(0, 1)

n = 8192
m = 1
data = np.zeros(n*m)

decimator.read_adc()
decimator.read_adc()

for i in range(m):
    print(i)
    data[8192*i:8192*i+8192] = decimator.read_adc()

plt.plot(data)
plt.show()