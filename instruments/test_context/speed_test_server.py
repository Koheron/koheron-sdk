#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import command, connect

import os
import numpy as np
import matplotlib.pyplot as plt
import time

class SpeedTest(object):
    def __init__(self, client):
        self.client = client
        self.wfm_size = 32 * 1024 / 4

    def call_method(self, method):
        if method == 'read_zeros':
            data = self.read_zeros()
        elif method == 'read_rambuf':
            data = self.read_rambuf()
        elif method == 'read_rambuf_memcpy':
            data = self.read_rambuf_memcpy()
        elif method == 'read_rambuf_std_copy':
            data = self.read_rambuf_std_copy()
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
        else:
            print 'unknown method'

    @command()
    def read_zeros(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32', check_type=False)

    @command()
    def read_rambuf(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32', check_type=False)

    @command()
    def read_rambuf_memcpy(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32', check_type=False)

    @command()
    def read_rambuf_std_copy(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32', check_type=False)

    @command()
    def read_rambuf_mycopy(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32', check_type=False)

    @command()
    def read_mmapbuf_nocopy(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32', check_type=False)

    @command()
    def read_rambuf_mmap_memcpy(self):
        return self.client.recv_array(2 * self.wfm_size, dtype='float32', check_type=False)

def read(driver, methods, n_pts=1000):
    n_methods = len(methods)
    time_total = np.zeros((n_pts, n_methods))
    for j, method in enumerate(methods):
        print method
        t0 = time.time()
        t_prev = t0
        for i in range(n_pts):
            driver.call_method(method)
            t = time.time()
            time_total[i,j] = t - t_prev
            t_prev = t
    return time_total

host = os.getenv('HOST', '192.168.1.100')
client = connect(host, name='test_context')
driver = SpeedTest(client)

methods = ('read_zeros',
           'read_rambuf',
           'read_rambuf_memcpy',
           'read_rambuf_std_copy',
           'read_rambuf_mycopy',
           'read_mmapbuf_nocopy',
           'read_rambuf_mmap_memcpy')

lineObjects = plt.plot(read(driver, methods, n_pts=1000))
plt.legend(iter(lineObjects), methods)
plt.ylabel('Time (s)')
plt.show()