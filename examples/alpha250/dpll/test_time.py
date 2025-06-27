import numpy as np
import matplotlib
matplotlib.use('GTKAgg')
from matplotlib import pyplot as plt
import os
import time
from koheron import connect, command

class Dpll(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_integrator(self, channel, integrator_index, integrator_on):
        pass

    @command()
    def set_dac_output(self, channel, sel):
        pass

    @command()
    def set_dds_freq(self, channel, freq):
        pass

    @command()
    def set_p_gain(self, channel, p_gain):
        pass

    @command()
    def set_pi_gain(self, channel, pi_gain):
        pass

    @command()
    def set_i2_gain(self, channel, i2_gain):
        pass

    @command()
    def set_i3_gain(self, channel, i3_gain):
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
    def set_cic_rate(self, val):
        pass

    @command(classname='Dma')
    def get_sampling_frequency(self):
        return self.client.recv_float()

    @command(classname='Dma')
    def get_data_size(self):
        return self.client.recv_uint32()

    @command(classname='Dma')
    def get_data(self):
        return self.client.recv_array(1000000, dtype='int32')


hosts = ['192.168.1.17', '192.168.1.22']
drivers = []

freqs = [10e6, 80e6]


for i, host in enumerate(hosts):
    drivers.append(Dpll(connect(host, 'dpll')))

for i, driver in enumerate(drivers):
    driver.set_reference_clock(0)
    driver.set_integrator(0, 0, True)
    driver.set_dds_freq(0, freqs[i])
    driver.set_dds_freq(1, freqs[(i+1)%2])
    driver.set_dac_output(0, 6)
    driver.set_dac_output(1, 7)

cic_rate = 20
n_avg = 100

driver.set_cic_rate()

n = driver.get_data_size()
fs = driver.get_sampling_frequency()

ffft = np.fft.fftfreq(n) * fs

y = np.ones(n)

# Dynamic plot
fig = plt.figure()
ax = fig.add_subplot(111)

li0, = ax.semilogx(np.fft.fftshift(ffft[1:n/2+1]), y[1:n/2+1], label="{} MHz carrier".format(freqs[0]*1e-6))
li1, = ax.semilogx(np.fft.fftshift(ffft[1:n/2+1]), y[1:n/2+1], label="{} MHz carrier".format(freqs[1]*1e-6))

li = [li0, li1]

ax.set_xlim((10, 1e6))
ax.set_ylim((-170, -70))
ax.set_xlabel('Frequency Offset (Hz)')
ax.set_ylabel('Phase Noise (dBc/Hz)')

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
#window = np.ones(n)
W = np.sum(window ** 2) # Correction factor for window

psd = np.zeros((2, n_avg, n))
i = 0

while True:
    i = (i + 1) % n_avg
    print i
    for j in range(2):
        data = drivers[j].get_data()
        data = data / 8192.0 * np.pi
        data -= np.mean(data)
        psd[j,i,:] = np.abs(np.fft.fft(window * data))**2
        psd[j,i,:] /= fs * W # rad^2/Hz
        mean_psd = np.mean(psd[j,:,:], axis=0)
        li[j].set_ydata(np.fft.fftshift(10*np.log10(mean_psd[1:n/2+1]/2)))
    fig.canvas.draw()
    plt.pause(0.001)
