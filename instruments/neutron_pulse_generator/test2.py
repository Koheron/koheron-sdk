#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
import os
import time

from neutronpulse import NeutronPulse
from koheron import connect

host = os.getenv('HOST', '10.210.1.44')
client = connect(host, name='neutron_pulse_generator')
driver = NeutronPulse(client)

# pulse_width = 128
# n_pulse = 64
# pulse_frequency = 1000

#pulse_period = np.uint32(driver.fs / pulse_frequency)
print('driver.fs is ' + str(driver.fs))
print('driver.n_pts is ' + str(driver.n_pts))

# Send Gaussian pulses to DACs
t = np.arange(driver.n_pts) / driver.fs # time grid (s)
driver.dac[0,:] = 0.6 * np.exp(-(t - 10000e-9)**2/(2000e-9)**2)
driver.dac[1,:] = 0.6 * np.exp(-(t - 10000e-9)**2/(2000e-9)**2)
driver.set_dac()

# driver.set_pulse_generator(pulse_width, pulse_period)

# n = pulse_width * n_pulse
n=1000000

# Dynamic plot
#fig = plt.figure()
#ax = fig.add_subplot(111)
#x = np.arange(n)
#y = np.zeros(n)
#li, = ax.plot(x, y)
#ax.set_ylim((0, 20000))
#ax.set_xlabel('FIFO sample number')
#ax.set_ylabel('ADC raw value')
#fig.canvas.draw()

adc0=np.int32([])
adc1=np.int32([])
adc0=np.empty(1000000,dtype=np.int32)
adc1=np.empty(1000000,dtype=np.int32)
 
while driver.get_fifo_length()==0:
    a=1
 
counter = 0
while True:
    data_available=driver.get_fifo_length()
    print(data_available)
    if data_available == 0: break
    data_rcv = driver.get_fifo_data(data_available)
    adc0[counter:counter+data_available]=(np.int32(data_rcv % (2**16)) )
    adc1[counter:counter+data_available]=(np.int32(data_rcv // (2**16)) )
    counter += data_available
 
print(counter)
 


plt.plot(adc1)
plt.show()


