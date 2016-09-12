#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np

class Sampling(object):
    """This class provides some methods to ease processing of signals sampled over time.
    """
    
    def __init__(self, n, fs):
        # Define sampling parameters
        self.n = n # Number of points in the waveform 'ex : n = 8192)
        self.fs = fs # Sampling frequency (Hz)
        self.dt = 1/self.fs # Time interval between samples 
        self.t = np.arange(self.n) * self.dt # Time frame
        self.df = self.fs/self.n # Frequency interval between frequency bins
        self.f_fft = np.fft.fftfreq(self.n) * self.fs # Frequency frame
        
    def shift_subpix(self, signal, tau, correct_slope=False):
        """Shift a signal of a delay `tau` using the FFT
        
        Args:
        	signal:
        	tau:
        	
        Note:
            `correct_slope` is used to remove the Gibbs phenomenon when the 
            signal is not periodic
        """
        wedge = np.exp(2*1j*np.pi*tau*self.f_fft)
        if correct_slope:
            signal_inter = signal[-1] + (signal[1]-signal[0]+signal[-1]-signal[-2])/2
            slope = (signal_inter - signal[0]) / (self.n * self.dt)
            signal = signal - slope * self.t
        fft_signal = np.fft.fft(signal)
        signal_shifted = np.real(np.fft.ifft(fft_signal * wedge))
        if correct_slope:
            signal_shifted += slope * self.t 
        return signal_shifted
