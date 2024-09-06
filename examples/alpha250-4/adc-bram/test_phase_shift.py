#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
import time
from adc_bram import AdcBram
from koheron import connect

import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
from matplotlib.lines import Line2D

def test_phase_shift(config):
    driver.get_adc(0) # Flush ADC buffer
    driver.get_adc(1) # Flush ADC buffer
    driver.set_sampling_frequency(config['idx'])

    n = 224
    data = np.zeros((4,n))

    for i in range(n):
        print(i)
        driver.phase_shift(1)
        driver.trigger_acquisition()
        time.sleep(0.1)
        driver.get_adc(0)
        driver.get_adc(1)
        data[0,i] = np.std(np.diff(driver.adc0[0]))
        data[1,i] = np.std(np.diff(driver.adc0[1]))
        data[2,i] = np.std(np.diff(driver.adc1[0]))
        data[3,i] = np.std(np.diff(driver.adc1[1]))

    plt.plot(data[0,:], label='IN0')
    plt.plot(data[1,:], label='IN1')
    plt.plot(data[2,:], label='IN2')
    plt.plot(data[3,:], label='IN3')
    plt.legend(loc='upper right')
    plt.title(config['name'])
    plt.show()

if __name__ == "__main__":
    host = os.getenv('HOST', '192.168.1.156')
    client = connect(host, 'adc-bram', restart=True)
    driver = AdcBram(client)

    driver.set_reference_clock(0) # External
    time.sleep(5)

    print('ADC size = {}'.format(driver.adc_size))

    clk_200MHz = {'name': '200 MHz', 'idx': 0, 'fs': 200E6}
    clk_250MHz = {'name': '250 MHz', 'idx': 1, 'fs': 250E6}
    clk_240MHz = {'name': '240 MHz', 'idx': 2, 'fs': 240E6}
    clk_100MHz = {'name': '100 MHz', 'idx': 3, 'fs': 100E6}

    # test_phase_shift(clk_200MHz)
    # test_phase_shift(clk_250MHz)
    # test_phase_shift(clk_240MHz)
    test_phase_shift(clk_100MHz)