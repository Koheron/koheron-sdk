#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib
matplotlib.use('GTKAgg')
from matplotlib import pyplot as plt
import os

from decimator import Decimator
from koheron import connect
from scipy import signal
from fir import get_taps


class Agilent33220A:
    def __init__(self, ip):
        import visa
        rm = visa.ResourceManager('@py')
        self.inst = rm.open_resource('TCPIP0::' + ip + '::INSTR')

    def set_sinus(self, freq, v_pp, offset):
        self.inst.write('APPL:SIN ' + str(freq / 1E3) + ' KHZ, '
        	            + str(v_pp / 2) + ' VPP, '
        	            + str(offset) + ' V\n')


gene = Agilent33220A('192.168.1.22')

host = os.getenv('HOST', '192.168.1.100')
client = connect(host, 'decimator')
driver = Decimator(client)

n = 8192
fs = 125e6

ffft = np.fft.fftfreq(n) * fs / 512

fidx = np.arange(1, 4096, 1)
freqs = ffft[fidx]
print freqs
gain = 0 * freqs

window = 0.5 * (1 + np.cos(2*np.pi*np.arange(n)/n))

for i, idx in enumerate(fidx):
    gene.set_sinus(freqs[i], 0.5, 0)

    for j in range(6):
        data = driver.read_adc()

    data = np.double(driver.read_adc())/2**31
    psd = np.abs(np.fft.fft(data * window))**2
    gain[i] += psd[idx]
    print freqs[i], gain[i]

N = 6.
R = 256.
M = 1.

taps, cic_response = get_taps(N, R, M)
w, h = signal.freqz(taps, worN=8192)
hh = np.fromiter(map(cic_response, w / R), dtype=np.float64)

gain_theo = abs(h * hh)**2
gain_theo /= max(gain_theo)

plt.plot(1e-3 * freqs, 10*np.log10(gain/max(gain)), label='Measured response')
plt.plot(1e-3 * freqs, 10*np.log10(gain_theo[fidx]), label='Theoretical response')
plt.ylim(-120, 10)
plt.legend(loc=3)
plt.xlabel('Frequency (kHz)')
plt.ylabel('Gain (dB)')
plt.show()
