#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
import time
from adc_dac_bram import AdcDacBram
from koheron import connect

import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
from matplotlib.lines import Line2D

def set_dac_modulation():
    amp_mod = 0.99
    t = np.arange(driver.dac_size) / driver.dac_size * 10
    driver.dac[0, :] = amp_mod * np.cos(2 * np.pi * t)
    driver.dac[1, :] = amp_mod * np.sin(2 * np.pi * t)
    driver.set_dac()

def test_phase_shift(config):
    driver.get_adc() # Flush ADC buffer
    driver.set_sampling_frequency(config['idx'])

    n = 224
    data = np.zeros((2,n))

    for i in range(n):
        print(i)
        driver.phase_shift(1)
        driver.get_adc()
        data[0,i] = np.std(np.diff(driver.adc[0]))
        data[1,i] = np.std(np.diff(driver.adc[1]))

    plt.plot(data[0,:], label='ADC0')
    plt.plot(data[1,:], label='ADC1')
    plt.legend(loc='upper right')
    plt.title(config['name'])
    plt.show()

if __name__ == "__main__":
    host = os.getenv('HOST', '192.168.1.42')
    client = connect(host, 'adc-dac-bram', restart=True)
    driver = AdcDacBram(client)

    print('DAC size = {}'.format(driver.dac_size))
    print('ADC size = {}'.format(driver.adc_size))
    set_dac_modulation()

    clk_200MHz = {'name': '200 MHz', 'idx': 0, 'fs': 200E6}
    clk_250MHz = {'name': '250 MHz', 'idx': 1, 'fs': 250E6}
    clk_100MHz = {'name': '100 MHz', 'idx': 2, 'fs': 100E6}
    clk_40MHz = {'name': '40 MHz', 'idx': 3, 'fs': 40E6}

    # test_phase_shift(clk_200MHz)
    # test_phase_shift(clk_250MHz)
    # test_phase_shift(clk_100MHz)
    test_phase_shift(clk_40MHz)