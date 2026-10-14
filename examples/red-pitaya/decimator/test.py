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

# Dynamic plot
fig = plt.figure()
ax = fig.add_subplot(111)
x = np.arange(n)
y = np.zeros(n)
li, = ax.plot(x, y)
ax.set_ylim((-2**31, 2**31))
fig.canvas.draw()


while True:
    try:
        data = driver.read_adc()
        print(driver.get_fifo_length())
        li.set_ydata(data)
        fig.canvas.draw()
        plt.pause(0.001)
    except KeyboardInterrupt:
        break
