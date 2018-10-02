import numpy as np
from scipy import signal
import matplotlib
# matplotlib.use('GTKAgg')
from matplotlib import pyplot as plt
import os
import time
from koheron import connect, command

class PhaseNoiseAnalyzer(object):
    def __init__(self, client):
        self.client = client

    @command(classname="Dds")
    def set_dds_freq(self, channel, freq):
        pass

    @command(classname="ClockGenerator")
    def set_tcxo_clock(self, val):
        pass

    @command(classname="ClockGenerator")
    def set_sampling_frequency(self, val):
        pass

    @command(classname="ClockGenerator")
    def set_reference_clock(self, val):
        pass

    @command(classname="ClockGenerator")
    def set_tcxo_clock(self, val):
        pass

    @command(classname='Dma')
    def get_data(self):
        return self.client.recv_array(1000000, dtype='int32')

    @command(classname='Dma')
    def get_phase_noise(self):
        return self.client.recv_vector(dtype='float32')


host = os.getenv('HOST','192.168.1.29')
freq = 40e6

driver = PhaseNoiseAnalyzer(connect(host, 'phase-noise-analyzer'))
driver.set_dds_freq(0,freq)

plt.semilogx(10*np.log10(driver.get_phase_noise()))
plt.show()

# fig = plt.figure()
# ax = fig.add_subplot(111)

# li, = ax.semilogx(np.fft.fftshift(ffft[1:n/2+1]), y[1:n/2+1], label="{} MHz carrier".format(freq*1e-6), linewidth=2)

# ax.set_xlim((10, 1e6))
# ax.set_ylim((-170, 0))
# ax.set_xlabel('Frequency Offset (Hz)')
# ax.set_ylabel('Phase Noise (dBc/Hz)')

# ax.legend(loc="upper right")

# ax.grid(True, which='major', linestyle='-', linewidth=1.5, color='0.35')
# ax.grid(True, which='minor', linestyle='-', color='0.35')
# ax.axhline(linewidth=2)
# ax.axvline(linewidth=2)
# ax.set_axisbelow(True)
# xlabels = ['', '10', '100', '1k', '10k', '100k', '1M']
# ax.set_xticklabels(xlabels)

# fig.canvas.draw()