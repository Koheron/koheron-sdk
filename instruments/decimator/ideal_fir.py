import sys
from scipy import signal
import numpy as np
import matplotlib.pyplot as plt

R = 4 # Decimation rate
ntaps_list = [16, 64, 256] # Number of coefficients in the FIR filter

for ntaps in ntaps_list:
    taps = signal.firwin2(ntaps, [0.0, 0.49/R, 0.51/R, 1.0], [1.0, 1.0, 0.0, 0.0])
    w, h = signal.freqz(taps, worN=16384)
    plt.plot(w / np.pi, 20 * np.log10(abs(h)), label='%d taps' % ntaps)

plt.ylabel('Gain (dB)')
plt.xlabel('Normalized frequency')
plt.ylim(-80, 20)
plt.legend()
plt.show()