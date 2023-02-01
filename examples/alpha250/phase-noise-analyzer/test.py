import numpy as np
from scipy import signal
import matplotlib
# matplotlib.use('GTKAgg')
from matplotlib import pyplot as plt
import os
import time
from koheron import connect, command
from phase_noise_analyzer import PhaseNoiseAnalyzer

host = os.getenv('HOST','192.168.1.42')
freq = 80e6
cic_rate = 20
channel = 1

driver = PhaseNoiseAnalyzer(connect(host, 'phase-noise-analyzer'))
driver.set_reference_clock(0)
driver.set_dds_freq(channel, freq)
driver.set_cic_rate(cic_rate)
driver.set_channel(channel)

f, psd_freq = driver.phase_noise(navg=20, verbose=True)

ax = plt.subplot(111)
ax.semilogx(f, psd_freq, linewidth=2)
ax.set_xlabel("FREQUENCY (Hz)")
ax.set_ylabel(u"PHASE NOISE (dBc/Hz)")
# ax.set_ylim(30, 100)
ax.set_xlim(1.0, 1E7)
ax.grid(True, which='major', linestyle='-', linewidth=1.5, color='0.35')
ax.grid(True, which='minor', linestyle='-', color='0.35')
xlabels = ['', '1', '10', '100', '1k', '10k', '100k', '1M', '10M']
ax.set_xticklabels(xlabels)
plt.show()
