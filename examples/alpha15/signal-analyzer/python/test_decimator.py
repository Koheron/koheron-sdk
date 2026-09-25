"""Plot both ALPHA15 signal analyzer decimated spectra."""

import os
import time

import matplotlib.pyplot as plt
import numpy as np

from decimator import Decimator
from koheron import connect


host = os.getenv('HOST', '192.168.1.105')
client = connect(host, 'signal-analyzer')
decimator = Decimator(client)
fs, fs_lf, _, transfer_duration_lf, _, _, n_pts = decimator.get_control_parameters()

decimator.spectral_density0()
decimator.spectral_density1()
time.sleep(20 * transfer_duration_lf)

psd = decimator.spectral_density0()
psd_lf = decimator.spectral_density1()
frequencies = np.arange(len(psd)) * fs / n_pts
frequencies_lf = np.arange(len(psd_lf)) * fs_lf / n_pts

plt.loglog(frequencies[1:], np.sqrt(psd[1:]), label='mid frequency')
plt.loglog(frequencies_lf[1:], np.sqrt(psd_lf[1:]), label='low frequency')
plt.xlabel('Frequency (Hz)')
plt.ylabel('Voltage noise density (V/√Hz)')
plt.legend()
plt.show()
