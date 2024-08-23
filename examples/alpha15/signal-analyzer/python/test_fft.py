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

host = os.getenv('HOST', '192.168.1.115')
client = connect(host, 'signal-analyzer')
fft = FFT(client)

fft.set_fft_window(1)

fft.select_adc_channel(0)
fft.range_select(0, 1)

freqs = np.arange(fft.n_pts/2) * 15. / 8192
psd = np.sqrt(fft.read_psd())

# Live fig

fig = plt.figure()
ax = fig.add_subplot(111)
li, = ax.loglog(freqs, psd)

fig.canvas.draw()

while True:
    try:
        psd = np.sqrt(fft.read_psd())
        li.set_ydata(psd)
        fig.canvas.draw()
        plt.pause(0.01)
    except KeyboardInterrupt:
        break