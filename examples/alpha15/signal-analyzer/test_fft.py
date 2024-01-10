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
fft = FFT(client)

fft.set_fft_window(1)
# fft.set_raw_window(65536 * np.ones(fft.n_pts, dtype=np.uint32))
fft.select_adc_channel(0)
fft.range_select(0, 1)

freqs = np.arange(fft.n_pts/2) * 15. / 8192
psd = np.sqrt(fft.read_psd_raw())

fig = plt.figure()
ax = fig.add_subplot(111)
# ax.set_xlim([0, 125])
# ax.set_ylim([0, 40])

# psd = np.sqrt(fft.read_psd()) # V/rtHz
ax.loglog(freqs, psd * 1e9, label='psd')

ax.set_xlabel('Frequency (MHz)')
ax.set_ylabel('Voltage noise density (U.A.)')
ax.legend()
plt.show()

# Live fig

fig = plt.figure()
ax = fig.add_subplot(111)
li, = ax.loglog(freqs, psd)

psd_min = np.amin(psd)
psd_max = np.amax(psd)

ax.set_ylim((0.9 * psd_min, 1.1 * psd_max))

# ax.set_xlim((100, 2e6))
ax.set_ylim((1E3, 1E10))
# ax.set_xlabel('FREQUENCY OFFSET (Hz)')
# ax.set_ylabel('PHASE NOISE (dBc/Hz)')

# ax.legend(loc="upper right")

# ax.grid(True, which='major', linestyle='-', linewidth=1.5, color='0.35')
# ax.grid(True, which='minor', linestyle='-', color='0.35')
# ax.axhline(linewidth=2)
# ax.axvline(linewidth=2)
# ax.set_axisbelow(True)
# xlabels = ['', '10', '100', '1k', '10k', '100k', '1M']
# ax.set_xticklabels(xlabels)
fig.canvas.draw()

time_prev = time.time()

print("start plot")
while True:
    try:
        psd = np.sqrt(fft.read_psd_raw())

        psd_min_ = np.amin(psd)
        psd_max_ = np.amax(psd)

        if psd_min_ < psd_min:
            psd_min = psd_min_
            ax.set_ylim((0.9 * psd_min, 1.1 * psd_max))

        if psd_max_ > psd_max:
            psd_max = psd_max_
            ax.set_ylim((0.9 * psd_min, 1.1 * psd_max))

        time_next = time.time()
        print(time_next - time_prev)
        li.set_ydata(psd)
        fig.canvas.draw()
        time_prev = time_next
        plt.pause(0.01)
    except KeyboardInterrupt:
        break