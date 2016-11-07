#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import command, connect
from oscillo import Oscillo

import os
import numpy as np
import matplotlib.pyplot as plt
import time

class SpeedTest(Oscillo):
    def __init__(self, client):
        super(SpeedTest, self).__init__(client)

    @command(classname='Oscillo')
    def read_all_channels_decim(self, decim_factor, index_low, index_high):
         self.tmp = self.client.recv_array(2 * (index_high - index_low) / decim_factor, dtype='uint32')

    def get_adc(self, method):
        if method == 'read_zeros':
            data = self.read_zeros()
        elif method == 'read_raw_all':
            data = self.read_raw_all()
        elif method == 'read_rambuf':
            data = self.read_rambuf()
        elif method == 'read_rambuf_memcpy':
            data = self.read_rambuf_memcpy()
        elif method == 'read_rambuf_mycopy':
            data = self.read_rambuf_mycopy()
        elif method == 'read_rambuf_nocpy':
            data = self.read_rambuf_nocpy()
        elif method == 'read_mmap_memcpy':
            data = self.read_mmap_memcpy()
        elif method == 'read_rambuf_mmap_memcpy':
            data = self.read_rambuf_mmap_memcpy()
        elif method == 'read_mmapbuf_nocopy':
            data = self.read_mmapbuf_nocopy()
        elif method == 'read_all_channels':
            data = self.read_all_channels()
        else:
            print 'unknown method'

        self.adc = np.reshape(data, (2, self.wfm_size))

    @command(classname='SpeedTest')
    def read_raw_all(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='uint32')

    @command(classname='SpeedTest')
    def read_zeros(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32')

    @command(classname='SpeedTest')
    def read_rambuf(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32')

    @command(classname='SpeedTest')
    def read_rambuf_memcpy(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32')

    @command(classname='SpeedTest')
    def read_rambuf_mycopy(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32')

    @command(classname='SpeedTest')
    def read_mmapbuf_nocopy(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32')

    @command(classname='SpeedTest')
    def read_rambuf_mmap_memcpy(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32')

def read(driver, methods, n_pts=1000):
    n_methods = len(methods)
    time_total = np.zeros((n_pts, n_methods))
    for j, method in enumerate(methods):
        t0 = time.time()
        t_prev = t0
        print j
        for i in range(n_pts):
            driver.get_adc(method)
            print i
            t = time.time()
            time_total[i,j] = t - t_prev
            t_prev = t
    return time_total


host = os.getenv('HOST','192.168.1.100')
client = connect(host, name='oscillo')
driver = SpeedTest(client)


methods = ('read_zeros',
           'read_rambuf',
           'read_rambuf_memcpy',
           'read_rambuf_mycopy',
           'read_mmapbuf_nocopy',
           'read_rambuf_mmap_memcpy')

lineObjects = plt.plot(read(driver, methods, n_pts=1000))
plt.legend(iter(lineObjects), methods)
plt.ylabel('Time (s)')
plt.show()

