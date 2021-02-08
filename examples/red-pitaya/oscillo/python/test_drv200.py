#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import connect
from oscillo import Oscillo
import os, time
import numpy as np
import matplotlib.pyplot as plt
import allantools

host = os.getenv('HOST','192.168.1.18')
client = connect(host, name='oscillo', restart=False)

driver = Oscillo(client)

driver.set_average(True)
driver.set_num_average_min(0)
driver.set_waveform_type(0, 1)
driver.set_dac_amplitude(0, 0.8)
driver.set_dac_frequency(0, 125e6/8192)
driver.set_dac_offset(0, 0.0)
data = driver.get_decimated_data(1, 0, 4096)

n = 10000

t = np.arange(n)
frequency = np.zeros(n)

fig = plt.figure()
ax = fig.add_subplot(111)

li, = ax.plot(t, frequency)
ax.set_xlabel('Time')
ax.set_ylabel('Frequency (MHz)')
fig.canvas.draw()
plt.show(block=False)

t0 = time.time()

n_pts = 500
x = np.linspace(-0.5, 0.5, num=2*n_pts)
# 2 GHz ~= 1250 pts
factor = 2 * n_pts * 2000/1250

while True:
    try:
        data = driver.get_decimated_data(1, 0, 4096)
        y = data[2048-n_pts:2048+n_pts]
        # Fit the absorption peak with a parabola y = ax^2+bx+c
        z = np.polyfit(x, y, 2)
        a = z[0]
        b = z[1]
        c = z[2]
        # Find the position of the peak minimum by derivating the parabola
        peak_pos = factor * -b/(2*a)

        frequency = np.roll(frequency, -1)
        frequency[-1] = peak_pos

        # Plot
        li.set_ydata(frequency)
        ax.relim()
        ax.autoscale_view(True, True, True)
        fig.canvas.draw()
        print(time.time() - t0, peak_pos)
        plt.pause(0.001)
    except KeyboardInterrupt:
        break

a = allantools.Dataset(data=frequency)
a.compute("adev")

# Plot it using the Plot class
b = allantools.Plot()
b.plot(a, errorbars=True, grid=True)
# You can override defaults before "show" if needed
b.ax.set_xlabel("Tau (s)")
b.show()