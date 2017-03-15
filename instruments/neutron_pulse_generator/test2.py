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

host = os.getenv('HOST', '10.210.1.45')
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
driver.dac[0,:] = np.append(0.9 * np.exp(-(np.arange(256)/125e6 - 1400e-9)**2/(800e-9)**2),0.315*np.exp(-np.arange(1024-256)/300))  #0.6 * np.exp(-(t - 3000e-9)**2/(1000e-9)**2)
driver.dac[1,:] = np.append(0.9 * np.exp(-(np.arange(256)/125e6 - 1400e-9)**2/(800e-9)**2),0.315*np.exp(-np.arange(1024-256)/300)) #0.6 * np.exp(-(t - 3000e-9)**2/(1000e-9)**2)
driver.set_dac()

# driver.set_pulse_generator(pulse_width, pulse_period)

# n = pulse_width * n_pulse
n=1000000	#Should be a multiple of 100

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
datablob=np.empty(n,dtype=np.int32)
data_available=0

while data_available==0:
    data_available=driver.get_fifo_length()
 


counter=0
while data_available!=0:
    datablob[counter:counter+data_available]=np.int32(driver.get_fifo_data(data_available))
    counter+=data_available
    data_available=driver.get_fifo_length()


adc0=(np.int32(datablob[0:counter] % (2**16)) )
adc1=(np.int32(datablob[0:counter] // (2**16)) )

 
print(str(counter))
np.savetxt('ADCdata',np.c_[adc0,adc1],fmt="%d",delimiter=",")


plt.plot(adc1)
plt.show()


