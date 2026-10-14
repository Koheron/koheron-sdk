#!/usr/bin/env python3

import numpy as np
from matplotlib import pyplot as plt
import os

from decimator import Decimator
from koheron import connect

host = os.getenv('HOST', '192.168.1.100')
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
