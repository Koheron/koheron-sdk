import numpy as np
from scipy import signal
from koheron import command

class PhaseNoiseAnalyzer(object):
    def __init__(self, client):
        self.client = client
        self.npts = 65536

    def set_dds_freq(self, channel, freq):
        """Compatibility alias that also invalidates analyzer acquisitions."""
        return self.set_local_oscillator(channel, freq)

    @command()
    def set_local_oscillator(self, channel, freq_hz):
        """Set the analyzer reference and discard settling data; DAC settings stay independent."""
        pass

    @command()
    def set_cic_rate(self, rate):
        self.fs = 125E6 / (2.0 * rate)

    @command()
    def get_parameters(self):
        return self.client.recv_tuple('IfIIIddIfI')

    @command()
    def get_phase_noise(self):
        """Current server spectrum in rad²/Hz, including DC and Nyquist."""
        return self.client.recv_vector(dtype='float32')

    @command()
    def set_tracking_enabled(self, enabled):
        """Track the selected ADC; disabling restores both nominal LO settings."""
        pass

    @command()
    def set_tracking_bandwidth(self, bandwidth_hz):
        pass

    @command()
    def set_tracking_max_step(self, max_step_hz):
        pass

    @command()
    def set_tracking_max_correction(self, max_correction_hz):
        pass

    @command()
    def get_tracking_parameters(self):
        """enabled, requested/effective BW, step/total limits, nominal LOs,
        applied corrections, measured LO-minus-input offsets, lock flags.
        Frequencies are Hz; the two-channel fields are ordered ADC0, ADC1.
        """
        return self.client.recv_tuple('?dddddddddd??')

    @command()
    def set_channel(self, channel):
        pass

    @command()
    def set_fft_navg(self, navg):
        pass

    @command()
    def save_config(self):
        pass

    @command()
    def get_carrier_power(self, navg):
        return self.client.recv_double()

    @command()
    def get_phase(self):
        # The server returns radians with CIC/FIR gain correction applied.
        return self.client.recv_array(self.npts, dtype='float32')

    @command()
    def set_phase_precision(self, bits):
        """Request 0–8 extra fractional bits, applied between complete packets."""
        return self.client.recv_bool()

    @command()
    def get_precision_status(self):
        """Requested/captured bits, rad/count, state (0 settling, 1 valid,
        2 overrange, 3 DMA error), valid captures, overranges, DMA errors,
        processing ms and capture period ms.
        """
        return self.client.recv_tuple('IIdIQQQdd')

    @command()
    def get_phase_snapshot(self):
        """One coherent snapshot: valid-capture count, precision, rad/count,
        validity and phase samples in radians.
        """
        metadata = self.client.recv_tuple('QIf?')
        # The tuple has one response header; its fixed array follows the scalars.
        samples = np.frombuffer(self.client.recv_all(4 * self.npts), dtype='<f4')
        return metadata, samples

    def phase_noise(self, navg=1, window='hann', verbose=False):
        if not isinstance(navg, (int, np.integer)) or navg < 1:
            raise ValueError("navg must be a positive integer")
        self.fs = self.get_parameters()[1]
        win = signal.get_window(window, Nx=self.npts)
        f = np.arange((self.npts // 2 + 1)) * self.fs / self.npts
        psd = np.zeros(f.size)

        power = 0

        for i in range(navg):
            if verbose:
                print("Acquiring sample {}/{}".format(i + 1, navg))

            phase = self.get_phase()
            psd += np.abs(np.fft.rfft(win * signal.detrend(phase, type="linear"))) ** 2
            power += self.get_carrier_power(40)

        if verbose:
            print(power / navg)

        psd /= navg
        psd /= (self.fs * np.sum(win ** 2)) # rad^2/Hz
        psd[1:-1] *= 2.0 # One-sided density; do not double DC or Nyquist

        # Divide by 2 because phase noise in dBc/Hz is defined as L = S_phi / 2
        # https://en.wikipedia.org/wiki/Phase_noise
        with np.errstate(divide="ignore"):
            psd_dB = 10.0 * np.log10(psd / 2.0) # dBc/Hz
        return f, psd_dB

    def frequency_noise(self, navg=1, window='hann', verbose=False):
        f, psd_dB = self.phase_noise(navg, window, verbose)
        with np.errstate(divide="ignore"):
            psd_freq = psd_dB + 10.0 * np.log10(2.0) + 20.0 * np.log10(f)
        return f, psd_freq
