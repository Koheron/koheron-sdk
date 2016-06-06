import context
import os
from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient, command
from project_config import ProjectConfig
import time
import numpy as np
import matplotlib.pyplot as plt

from drivers.common import Common
from drivers.pid import Pid

host = os.getenv('HOST','192.168.1.100')
project = os.getenv('NAME','')

im = InstrumentManager(host)
im.install_instrument(project)
client = KClient(host)

pc = ProjectConfig(project)

class Test:

    def __init__(self, client):
        self.client = client

        self.common = Common(client)
        self.pid = Pid(client)
       
driver = Test(client)

driver.common.status()


freq = 0 # MHz
fs = 125e6 # Sampling frequency

driver.pid.set_dds_freq(freq)

n = 32768
dec_factor_list = [64, 256, 1024, 4096]
n_avg = 10

psd = np.zeros((len(dec_factor_list), n))
f_fft = 0 * psd

for j in range(n_avg):
    for i, dec_factor in enumerate(dec_factor_list):
        print i, j
        driver.pid.set_cic_rate(dec_factor)
        time.sleep(0.001)
        data = ((driver.pid.read_npts_fifo(n) - 2**23) % 2**24 - 2**23)
        psd[i,:] += np.abs(np.fft.fft(data))**2 * dec_factor
        f_fft[i,:] = np.fft.fftfreq(n) * fs / dec_factor

psd /= n_avg

plt.figure()
plt.hold(True)
for i, dec_factor in enumerate(dec_factor_list):
    plt.semilogx(np.fft.fftshift(f_fft[i,:]), np.fft.fftshift(10*np.log10(psd[i,:])), label=str(dec_factor))

plt.xlabel('Frequency (Hz)')
plt.ylabel('Power spectral density (dB)')
plt.legend()
plt.show()