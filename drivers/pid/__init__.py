# -*- coding: utf-8 -*-

import time
import numpy as np
import matplotlib.pyplot as plt

from koheron_tcp_client import command, write_buffer

class Pid(object):

    def __init__(self, client, acq_period=100):
        self.client = client
        if self.open() < 0:
            print "Cannot open PID driver"
        self.fifo_start_acquisition(acq_period)
        self.data_remains = []

    @command('PID')
    def open(self):
        return self.client.recv_int32()

    @command('PID','I')
    def set_cic_rate(self, rate):
        pass

    @command('PID','f')
    def set_dds_freq(self, freq):
        pass

    @command('PID')
    def get_fifo_length(self):
        return self.client.recv_uint32()

    def get_data(self):
        @command('PID')
        def get_fifo_buffer_length(self):
            return self.client.recv_uint32()

        self.fifo_stream_length = get_fifo_buffer_length(self)

        if self.fifo_stream_length > 0:
            # print "stream_length = " + str(self.fifo_stream_length)

            @command('PID')
            def get_fifo_data(self):
                return self.client.recv_buffer(self.fifo_stream_length, data_type='uint32')

            return get_fifo_data(self)
        else:
            return []

    @command('PID')
    def fifo_get_acquire_status(self):
        return self.client.recv_uint32()

    @command('PID','I')
    def fifo_start_acquisition(self, acq_period): pass

    @command('PID')
    def fifo_stop_acquisition(self): pass

    def read_npts_fifo(self, npts):
        if len(self.data_remains) >= npts:
            data = self.data_remains[:npts]
            self.data_remains = self.data_remains[npts+1:-1]
            return data

        if len(self.data_remains) == 0:
            data = np.zeros(npts)
            idx = 0
        else:
            data = np.zeros(npts)
            data[:len(self.data_remains)] = np.copy(self.data_remains)
            idx = len(self.data_remains)

        while idx < npts:
            data_rcv = self.get_data()
            if self.fifo_stream_length > 0:
                if idx + self.fifo_stream_length >= npts:
                    data[idx:npts-1] = data_rcv[:npts-idx-1]
                    self.data_remains = data_rcv[npts-idx:-1]
                    return data
                else:
                    data[idx:idx+self.fifo_stream_length] = data_rcv
                    idx += self.fifo_stream_length