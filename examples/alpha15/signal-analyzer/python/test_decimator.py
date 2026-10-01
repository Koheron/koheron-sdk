#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
from matplotlib import pyplot as plt
import os
import time

from decimator import Decimator
from fft import FFT
from koheron import connect

host = os.getenv('HOST', '192.168.1.85')
client = connect(host, 'signal-analyzer')

decimator = Decimator(client)
fft = FFT(client)

fft.select_adc_channel(0)
fft.range_select(0, 1)

n = 32768
m = 1
data = np.zeros(n*m)

decimator.read_adc()
decimator.read_adc()

for i in range(m):
    print(i)
    data[n*i:n*(i+1)] = decimator.read_adc()

plt.plot(data)
plt.show()