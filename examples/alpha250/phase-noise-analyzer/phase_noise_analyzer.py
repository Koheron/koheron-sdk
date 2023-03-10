import numpy as np
from scipy import signal
from koheron import command

class PhaseNoiseAnalyzer(object):
    def __init__(self, client):
        self.client = client
        self.calib_factor = 4.196
        self.npts = 200000

    @command(classname="Dds")
    def set_dds_freq(self, channel, freq):
        pass

    @command(classname="ClockGenerator")
    def set_reference_clock(self, val):
        pass

    @command()
    def set_cic_rate(self, rate):
        self.fs = 200E6 / (2.0 * rate)

    @command()
    def set_channel(self, channel):
        pass

    @command()
    def get_carrier_power(self, navg):
        return self.client.recv_float()

    @command()
    def get_data(self):
        return self.client.recv_array(self.npts, dtype='int32')

    @command()
    def get_phase(self):
        return self.client.recv_array(self.npts, dtype='float32')

    def phase_noise(self, navg=1, window='hann', verbose=False):
        win = signal.get_window(window, Nx=self.npts)
        f = np.arange((self.npts // 2 + 1)) * self.fs / self.npts
        psd = np.zeros(f.size)

        power = 0

        for i in range(navg):
            if verbose:
                print("Acquiring sample {}/{}".format(i + 1, navg))

            phase = self.get_phase()
            psd += 2.0 * np.abs(np.fft.rfft(win * (phase - np.mean(phase)))) ** 2
            power += self.get_carrier_power(40)

        print(power / navg)

        psd /= navg
        psd /= (self.fs * np.sum(win ** 2)) # rad^2/Hz

        # Divide by 2 because phase noise in dBc/Hz is defined as L = S_phi / 2 
        # https://en.wikipedia.org/wiki/Phase_noise
        psd_dB = 10.0 * np.log10(psd / 2.0) # dBc/Hz
        return f, psd_dB

    def frequency_noise(self, navg=1, window='hann', verbose=False):
        f, psd_dB = self.phase_noise(navg, window, verbose)
        psd_freq = psd_dB + 3.0 + 20.0 * np.log10(f)
        return f, psd_freq