#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib
matplotlib.use('GTKAgg')
from matplotlib import pyplot as plt

from scipy import signal
import os, time
from koheron import connect
from fft import FFT

if __name__=="__main__":
    host = os.getenv('HOST', '192.168.1.16')
    client = connect(host, 'fft')
    driver = FFT(client)

    driver.set_dds_freq(0, 50e6)


    n = 4096
    fs = 250e6
    cic_rate = 500

    ffft = np.fft.fftfreq(n) * fs / (cic_rate * 2)

    # Dynamic plot
    fig = plt.figure()
    ax = fig.add_subplot(111)
    x = np.arange(n)
    y = np.zeros(n)

    li, = ax.plot(np.fft.fftshift(ffft), y)
    ax.set_ylim((-1000, 1000))

    fig.canvas.draw()

    window = 0.5 * (1 - np.cos(2*np.pi*np.arange(n)/n))

    n_avg = 1
    dsp = np.zeros((n_avg, n))
    i = 0

    while True:
        try:
            i = (i + 1) % n_avg
            print(driver.get_fifo_length())
            data = driver.get_vector(2*n)
            z = data[::2] + 1j*data[1::2]

            dsp[i,:] =  np.abs(np.fft.fft(z))**2

            mean_dsp = np.mean(dsp, axis=0)

            li.set_ydata(np.fft.fftshift(10*np.log10(mean_dsp)))

            fig.canvas.draw()
            plt.pause(0.001)

        except KeyboardInterrupt:
            break