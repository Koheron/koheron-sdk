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
    fs_dac = 240E6
    ndac = driver.dac_size
    t_dac = np.arange(ndac) / fs_dac
    f_dac = 15E6 / 50
    f_dac = np.round(f_dac / fs_dac * ndac) * fs_dac / ndac
    driver.dac[0, :] = amp_mod * np.sin(2.0 * np.pi * f_dac * t_dac)
    driver.dac[1, :] = amp_mod * np.sin(2.0 * np.pi * f_dac * t_dac)
    driver.set_dac()

def test_phase_shift():
    # driver.enable_adcs()
    # driver.clear_testpat()
    driver.set_testpat()

    input_range = 0
    driver.range_select(0, input_range)
    driver.range_select(1, input_range)

    driver.get_adc(0)
    driver.get_adc(1)

    # n = 300
    n = 32
    data = np.zeros((2,n))

    for i in range(n):
        print(i)
        # driver.phase_shift(1)
        driver.dco_delay_tap(0, i)
        # driver.da_delay_tap(0, i)
        # driver.db_delay_tap(0, i)
        # driver.dco_delay_tap(1, i)
        # driver.da_delay_tap(1, i)
        # driver.db_delay_tap(1, i)
        time.sleep(0.1)

        driver.get_adc(0)
        driver.get_adc(1)
        adc0 = driver.adc[0,:]
        adc1 = driver.adc[1,:]
        # plt.plot(adc0)
        # plt.plot(adc1)
        # plt.show()

        data[0,i] = np.std(np.diff(adc0))
        data[1,i] = np.std(np.diff(adc1))
        # n = driver.adc_size
        # data[0,i] = np.std(np.diff(driver.adc[0,2 * n // 3:-1]))
        # data[1,i] = np.std(np.diff(driver.adc[1,2 * n // 3:-1]))
        # data[0,i] = driver.adc[0][0]
        # data[1,i] = driver.adc[1][0]
        
    plt.plot(data[0,:], label='ADC0')
    plt.plot(data[1,:], label='ADC1')
    plt.legend(loc='upper right')
    plt.show()

if __name__ == "__main__":
    host = os.getenv('HOST', '192.168.1.113')
    client = connect(host, 'adc-dac-bram', restart=True)
    driver = AdcDacBram(client)
    set_dac_modulation()
    test_phase_shift()
