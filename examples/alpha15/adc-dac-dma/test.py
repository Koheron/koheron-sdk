#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
from koheron import command, connect
import matplotlib.pyplot as plt
import numpy as np
from adc_dac_dma import AdcDacDma


if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.113')
    client = connect(host, name='adc-dac-dma')
    driver = AdcDacDma(client)

    # -------------------------------------------------------------------------------
    # Set ADC
    # -------------------------------------------------------------------------------

    input_range = 0

    driver.range_select(0, input_range)
    driver.range_select(1, input_range)

    if input_range == 0:
        input_span = 2.048 # Vpp
    else:
        input_span = 8.192 # Vpp

    adc_channel = 0
    driver.select_adc_channel(adc_channel)

    # -------------------------------------------------------------------------------
    # Set chirp on DAC
    # -------------------------------------------------------------------------------

    fs_dac = 240E6 # Hz
    fmin = 1E1     # Hz
    fmax = 1E3     # Hz

    t_dac = np.arange(driver.n) / fs_dac
    chirp = (fmax - fmin) / (t_dac[-1] - t_dac[0])
    # chirp = 0.0

    phase = np.random.random() * 2 * np.pi

    print("Set DAC waveform (chirp between {} and {} kHz)".format(1E-3 * fmin, 1E-3 * fmax))
    driver.dac = 0.9 * np.cos(2.0 * np.pi * (fmin + chirp * t_dac) * t_dac + phase)
    driver.set_dac()

    # -------------------------------------------------------------------------------
    # Run DMA
    # -------------------------------------------------------------------------------

    fs_adc = 15E6
    adc = np.zeros(driver.n)

    print("Get ADC{} data ({} points)".format(adc_channel, driver.n))
    driver.start_dma()
    time.sleep(driver.n / fs_adc)
    driver.get_adc()
    driver.stop_dma()

    n_pts = 4*1024*1024
    t_adc = np.arange(n_pts) / fs_adc
    print("Plot first {} points".format(n_pts))
    plt.plot(1E3 * t_adc, driver.adc[0:n_pts] * input_span / 2**18)
    plt.ylim((-input_span / 2.0, +input_span / 2.0))
    plt.xlabel('TIME (ms)')
    plt.ylabel('ADC SIGNAL (V)')
    plt.show()
