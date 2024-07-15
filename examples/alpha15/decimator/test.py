#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
import os
import time

from decimator import Decimator
from koheron import connect

host = os.getenv('HOST', '192.168.1.129')
client = connect(host, 'decimator')
driver = Decimator(client)

n = 8192
m = 2
data = np.zeros(n*m)

driver.read_adc()
driver.read_adc()

for i in range(m):
    print(i)
    data[8192*i:8192*i+8192] = driver.read_adc()

plt.plot(data)
plt.show()