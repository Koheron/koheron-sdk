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

    @command()
    def get_data(self):
        return self.client.recv_array(1000000, dtype='int32')


host = os.getenv('HOST','192.168.1.29')
freq = 40e6

driver = PhaseNoiseAnalyzer(connect(host, 'phase-noise-analyzer'))
driver.set_sampling_frequency(0)
driver.set_reference_clock(2)
driver.set_dds_freq(0,freq)

n = 1000000
fs = 200e6
cic_rate = 20
n_avg = 1

ffft = np.fft.fftfreq(n) * fs / (cic_rate * 2)

y = np.ones(n)

# Dynamic plot
fig = plt.figure()
ax = fig.add_subplot(111)

li, = ax.semilogx(np.fft.fftshift(ffft[1:n/2+1]), y[1:n/2+1], label="{} MHz carrier".format(freq*1e-6), linewidth=2)

ax.set_xlim((10, 1e6))
ax.set_ylim((-170, 0))
ax.set_xlabel('FREQUENCY OFFSET (Hz)')
ax.set_ylabel('PHASE NOISE (dBc/Hz)')

ax.legend(loc="upper right")

ax.grid(True, which='major', linestyle='-', linewidth=1.5, color='0.35')
ax.grid(True, which='minor', linestyle='-', color='0.35')
ax.axhline(linewidth=2)
ax.axvline(linewidth=2)
ax.set_axisbelow(True)
xlabels = ['', '10', '100', '1k', '10k', '100k', '1M']
ax.set_xticklabels(xlabels)

fig.canvas.draw()

window = 0.5 * (1 - np.cos(2*np.pi*np.arange(n)/(n-1)))

window = np.ones(n)

#window = signal.blackmanharris(n)
#window = 0.5 * (1 - np.cos(2*np.pi*np.arange(n)/(n-1)))
#window = signal.nuttall(n)
window = signal.chebwin(n, at=200)

W = np.sum(window ** 2) # Correction factor for window

psd = np.zeros((n_avg, n))
i = 0

while True:
    try:
        i = (i + 1) % n_avg
        data = driver.get_data()
        print i, np.mean(data)
        data = data / 8192.0 * np.pi
        #data = data - data[0] - x * (data[-1] - data[0])
        data -= np.mean(data)
        psd[i,:] = np.abs(np.fft.fft(window * data))**2
        psd[i,:] /= (fs / (cic_rate * 2) * W) # rad^2/Hz
        mean_psd = np.mean(psd, axis=0)
        li.set_ydata(np.fft.fftshift(10*np.log10(mean_psd[1:n/2+1]/2)))
        fig.canvas.draw()
        # np.save('phase-noise-alpha250.npy', [np.fft.fftshift(ffft[1:n/2+1]), np.fft.fftshift(10*np.log10(mean_psd[1:n/2+1]/2))])
        plt.pause(0.001)
    except KeyboardInterrupt:
        break

