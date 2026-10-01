import sys
from scipy import signal
import numpy as np

# https://www.altera.com/en_US/pdfs/literature/an/an455.pdf
# http://dsp.stackexchange.com/questions/160/fir-filter-compensator-when-using-a-cic-decimation-filter
# http://www.acasper.org/2011/10/02/my-sdr/

def get_taps(N, R, M, ntaps=128, cutoff=0.40):
    """
    Find the coefficients of the half-band FIR filter that compensate the CIC filter from 0 to cutoff
    N : number of CIC stages
    R : decimation rate
    M : differential delay in the comb section stages of the filter
    """
    f = np.arange(2048) / 2047.
    cic_response = lambda f : abs( M/R * (np.sin((f*R)/2)) / (np.sin((f*M)/2.)) )**N if f !=0 else 1

    H = np.fromiter(map(cic_response, f*np.pi), dtype=np.float64)

    # Define frequency reponse of ideal compensation filter
    H = np.fromiter(map(cic_response, f*np.pi / R), dtype=np.float64)
    Hc = 1/H * (f < cutoff)

    beta = 8
    taps = signal.firwin2(ntaps, f, Hc, nfreqs = 1025, window=('kaiser', beta))
    taps /= np.sum(taps)
    return taps, cic_response

if __name__=="__main__":

    N = float(sys.argv[1]) # number of CIC stages
    R = float(sys.argv[2]) # decimation rate
    M = float(sys.argv[3]) # differential delay

    ntaps = 128
    cutoff = 0.40

    if (len(sys.argv) >= 5):
        if sys.argv[4] != 'print':
            ntaps = int(sys.argv[4])

        if (len(sys.argv) >= 6):
            cutoff = float(sys.argv[5])

    taps, cic_response = get_taps(N, R, M, ntaps, cutoff)

    if sys.argv[-1] == 'print':
        print(', '.join('%e' % f for f in taps))

    if sys.argv[-1] == 'plot':
        import matplotlib.pyplot as plt
        w, h = signal.freqz(taps, worN=16384)
        hh = np.fromiter(map(cic_response, w / R), dtype=np.float64)
        hh[0] = 1
        plt.plot(w / np.pi, 20 * np.log10(abs(h)), label='FIR filter')
        plt.plot(w / np.pi, 20 * np.log10(abs(h * hh)), label='Compensated filter')
        plt.plot(w / np.pi, 20 * np.log10(abs(hh)), label='CIC filter')
        plt.xlabel('Normalized frequency')
        plt.ylabel('Filter gain (dB)')
        plt.legend()
        plt.ylim(-120, 20)
        plt.show()