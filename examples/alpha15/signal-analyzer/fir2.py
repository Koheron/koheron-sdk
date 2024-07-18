import sys
from scipy import signal
import numpy as np

# https://www.altera.com/en_US/pdfs/literature/an/an455.pdf
# http://dsp.stackexchange.com/questions/160/fir-filter-compensator-when-using-a-cic-decimation-filter
# http://www.acasper.org/2011/10/02/my-sdr/

def get_taps(ntaps=16, cutoff=0.1):
    """
    Find the coefficients of the half-band FIR filter that compensate the CIC filter from 0 to cutoff
    N : number of CIC stages
    R : decimation rate
    M : differential delay in the comb section stages of the filter
    """
    f = np.arange(2048) / 2047.
    H = np.ones(f.size) * (f < cutoff)

    beta = 8
    taps = signal.firwin2(ntaps, f, H, nfreqs = 1025, window=('kaiser', beta))
    taps /= np.sum(taps)
    return taps

if __name__=="__main__":

    taps = get_taps()
    print(taps)

    import matplotlib.pyplot as plt
    w, h = signal.freqz(taps, worN=16384)
    plt.plot(w / np.pi, 20 * np.log10(abs(h)), label='FIR filter')
    plt.xlabel('Normalized frequency')
    plt.ylabel('Filter gain (dB)')
    plt.legend()
    plt.ylim(-120, 20)
    plt.show()
