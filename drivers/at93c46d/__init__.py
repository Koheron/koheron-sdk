# -*- coding: utf-8 -*-
import time
import numpy as np

from koheron_tcp_client import command, write_buffer

class At93c46d(object):

    def __init__(self, client):
        self.client = client

        if self.open() < 0:
            print('Cannot open device AT93C46D')
       
    @command('AT93C46D')
    def open(self):
        return self.client.recv_int32()

    @command('AT93C46D','I')
    def read(self, addr):
        return self.client.recv_uint32()

    @command('AT93C46D')
    def write_enable(self):
        pass

    @command('AT93C46D','I')
    def erase(self, addr):
        pass

    @command('AT93C46D','II')
    def write(self, addr, data_in):
        pass

    @command('AT93C46D')
    def erase_all(self):
        pass

    @command('AT93C46D','I')
    def write_all(self, data_in):
        pass

    @command('AT93C46D')
    def erase_write_disable(self):
        pass

    def test(self, verbose=True):
        if verbose:
            print('Testing AT93C46D driver')
        self.write_enable()
        addr = 12
        val = 42
        for i in range(10):
            self.write(addr, i)
            time.sleep(0.002)
            assert(self.read(addr) == i)