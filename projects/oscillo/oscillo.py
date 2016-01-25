#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from base import Base
from koheron_tcp_client import command

class Oscillo(Base):
    """ Driver for the oscillo bitstream
    """

    def __init__(self, client, verbose=False):
        self.wfm_size = 8192
        super(Oscillo, self).__init__(self.wfm_size, client)

        self.open(self.wfm_size)
       
        self.avg_on = False

        self.adc = np.zeros((2, self.wfm_size))
        self.spectrum = np.zeros((2, self.wfm_size / 2))
        self.avg_spectrum = np.zeros((2, self.wfm_size / 2))
        self.ideal_amplitude_waveform = np.zeros(self.wfm_size)
        self.amplitude_transfer_function = np.ones(self.wfm_size,
                                                   dtype=np.dtype('complex64'))

        # Correction
        sigma_freq = 5e6  # Hz
        self.gaussian_filter = np.exp(-self.sampling.f_fft ** 2 / (2 * sigma_freq ** 2))

        # Calibration
        self.adc_offset = np.zeros(2)
        self.optical_power = np.ones(2)
        self.power = np.ones(2)

        self.reset()

    @command('OSCILLO')
    def open(self, wfm_size):
        pass

    def reset(self):
        super(Oscillo, self).reset()
        self.avg_on = False
        self.set_averaging(self.avg_on)

    def set_averaging(self, avg_status):
        """ Enable/disable averaging

        Args:
            avg_status: Status ON or OFF
        """
        if avg_status:
            status = 1;
        else:
            status = 0;

        @command('OSCILLO')
        def set_averaging(self, status):
            pass

        set_averaging(self, status)

    @command('OSCILLO')
    def get_num_average(self):
        n_avg = self.client.recv_int(4)

        if math.isnan(n_avg):
            print("Can't read laser power")
            self.failed = True

        return n_avg

    @command('OSCILLO')
    def read_all_channels(self):
        """ Read all the acquired channels """
        return self.client.recv_buffer(2 * self.wfm_size,
                                       data_type='float32')
        # TODO Check reception

    def get_adc(self):
        data = self.read_all_channels()

        self.adc[0, :] = data[0:self.wfm_size]
        self.adc[1, :] = data[self.wfm_size:]

        self.adc[0, :] -= self.adc_offset[0]
        self.adc[1, :] -= self.adc_offset[1]
        self.adc[0, :] *= self.optical_power[0] / self.power[0]
        self.adc[1, :] *= self.optical_power[1] / self.power[1]

    def _white_noise(self, n_freqs, n_stop=None):
        if n_stop is None:
            n_stop = n_freqs
        amplitudes = np.zeros(n_freqs)
        amplitudes[0:n_stop] = 1
        random_phases = 2 * np.pi * np.random.rand(n_freqs)
        white_noise = np.fft.irfft(amplitudes * np.exp(1j * random_phases))
        white_noise = np.fft.fft(white_noise)
        white_noise[0] = 0.01
        white_noise[self.wfm_size / 2] = 1
        white_noise = np.real(np.fft.ifft(white_noise))
        white_noise /= 1.7 * np.max(np.abs(white_noise))
        return white_noise

    def get_amplitude_transfer_function(self, channel_dac=0,
                                        channel_adc=0, transfer_avg=100):
        n_freqs = self.wfm_size / 2 + 1
        self.amplitude_transfer_function *= 0

        for i in range(transfer_avg):
            white_noise = self._white_noise(n_freqs)
            self.dac[channel_dac, :] = white_noise
            self.set_dac()
            time.sleep(0.01)
            self.get_adc()
            self.amplitude_transfer_function += np.fft.fft(self.adc[channel_adc, :]) / np.fft.fft(white_noise)
        self.amplitude_transfer_function = self.amplitude_transfer_function / transfer_avg
        self.amplitude_transfer_function[0] = 1
        self.dac[channel_dac, :] = np.zeros(self.wfm_size)
        self.set_dac()

    def get_correction(self):
        tmp = np.fft.fft(self.amplitude_error) / self.amplitude_transfer_function
        tmp[0] = 0
        tmp = self.gaussian_filter * tmp
        return np.real(np.fft.ifft(tmp))

    def optimize_amplitude(self, alpha=1, channel=0):
        self.amplitude_error = (self.adc[0, :] - np.mean(self.adc[0, :])) - self.ideal_amplitude_waveform
        self.dac[channel, :] -= alpha * self.get_correction()

    def get_spectrum(self):
        fft_adc = np.fft.fft(self.adc, axis=1)
        self.spectrum = fft_adc[:, 0:self.wfm_size / 2]

    def get_avg_spectrum(self, n_avg=1):
        self.avg_spectrum = np.zeros((2, self.wfm_size / 2))
        for i in range(n_avg):
            self.get_adc()
            fft_adc = np.unwrap(np.angle(np.fft.fft(self.adc, axis=1)))
            self.avg_spectrum += fft_adc[:, 0:self.wfm_size / 2]

        self.avg_spectrum = self.avg_spectrum / n_avg

    def set_amplitude_transfer_function(self, amplitude_transfer_function):
        self.amplitude_transfer_function = amplitude_transfer_function

