import numpy as np
from scipy import signal
import matplotlib
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
    def set_reference_clock(self, val):
        pass

    @command()
    def get_data(self):
        return self.client.recv_array(1000000, dtype='int32')

    @command()
    def start(self):
        pass

    @command()
    def get_parameters(self):
        return self.client.recv_tuple('If')

    @command()
    def get_phase_noise(self, n_avg):
        return self.client.recv_vector(dtype='float32')


host = os.getenv('HOST','192.168.1.29')
freq = 10e6 # Hz

driver = PhaseNoiseAnalyzer(connect(host, 'phase-noise-analyzer'))
driver.set_reference_clock(0)
driver.set_dds_freq(0, freq)
print("get parameters")
(n, fs) = driver.get_parameters()

print(n, fs)

fig = plt.figure()
ax = fig.add_subplot(111)
f = np.arange(n) * fs / n / 2
li, = ax.semilogx(f, np.ones(n), label="{} MHz carrier".format(freq*1e-6), linewidth=2)

ax.set_xlim((100, 2e6))
ax.set_ylim((-200, 0))
ax.set_xlabel('FREQUENCY OFFSET (Hz)')
ax.set_ylabel('PHASE NOISE (dBc/Hz)')

ax.legend(loc="upper right")

ax.grid(True, which='major', linestyle='-', linewidth=1.5, color='0.35')
ax.grid(True, which='minor', linestyle='-', color='0.35')
ax.axhline(linewidth=2)
ax.axvline(linewidth=2)
ax.set_axisbelow(True)
xlabels = ['', '10', '100', '1k', '10k', '100k', '1M']
# ax.set_xticklabels(xlabels)
fig.canvas.draw()

time_prev = time.time()

print("Start acquiisition")
driver.start()

print("start plot")
while True:
    try:
        data = driver.get_phase_noise(1)
        time_next = time.time()
        print(time_next - time_prev)
        li.set_ydata(10 * np.log10(data))
        fig.canvas.draw()
        time_prev = time_next
        plt.pause(0.001)
    except KeyboardInterrupt:
        break