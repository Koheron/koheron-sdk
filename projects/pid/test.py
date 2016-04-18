from config import *

import time
import numpy as np
import matplotlib.pyplot as plt

def set_cic_rate(rate):
    dvm.write(CONFIG, CIC_RATE_OFF, rate)

class Pid(object):

    def __init__(self, client, acq_period=100):
        self.client = client
        if self.open() < 0:
            print "Cannot open driver"
        self.fifo_start_acquisition(acq_period)
        self.data_remains = []

    @command('PID')
    def open(self):
        return self.client.recv_int(4)

    @command('PID')
    def get_fifo_length(self):
        return self.client.recv_int(4)

    def get_data(self):
        @command('PID')
        def store_fifo_data(self):
            return self.client.recv_int(4)

        self.fifo_stream_length = store_fifo_data(self)

        if self.fifo_stream_length > 0:
            # print "stream_length = " + str(self.fifo_stream_length)

            @command('PID')
            def get_fifo_data(self):
                return self.client.recv_buffer(self.fifo_stream_length, data_type='uint32')

            return get_fifo_data(self)
        else:
            return []

    @command('PID')
    def fifo_get_acquire_status(self):
        return self.client.recv_int(4)

    @command('PID')
    def fifo_start_acquisition(self, acq_period):
        pass

    @command('PID')
    def fifo_stop_acquisition(self):
        pass

    def read_npts_fifo(self, npts):
        if len(self.data_remains) >= npts:
            data = self.data_remains[:npts]
            self.data_remains = self.data_remains[npts+1:-1]
            return data

        if len(self.data_remains) == 0:
            data = np.zeros(npts)
            idx = 0
        else:
            data = np.zeros(npts)
            data[:len(self.data_remains)] = np.copy(self.data_remains)
            idx = len(self.data_remains)

        while idx < npts:
            data_rcv = self.get_data()
            if self.fifo_stream_length > 0:
                if idx + self.fifo_stream_length >= npts:
                    data[idx:npts-1] = data_rcv[:npts-idx-1]
                    self.data_remains = data_rcv[npts-idx:-1]
                    return data
                else:
                    data[idx:idx+self.fifo_stream_length] = data_rcv
                    idx += self.fifo_stream_length

    def __del__(self):
        self.fifo_stop_acquisition()

if __name__ == "__main__":
    freq = 10 # MHz
    fs = 125e6 # Sampling frequency
    dvm.write(CONFIG, DDS_OFF, np.floor(freq / fs * 2**32))

    driver = Pid(client)

    n = 32768
    dec_factor_list = [64, 256, 1024, 4096]
    n_avg = 10

    psd = np.zeros((len(dec_factor_list), n))
    f_fft = 0 * psd

    for j in range(n_avg):
        for i, dec_factor in enumerate(dec_factor_list):
            print i, j
            set_cic_rate(dec_factor)
            time.sleep(0.001)
            data = ((driver.read_npts_fifo(n) - 2**23) % 2**24 - 2**23)
            # print len(data)
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