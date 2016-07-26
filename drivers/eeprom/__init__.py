# -*- coding: utf-8 -*-
import time
import numpy as np

from koheron_tcp_client import command, write_buffer

class Eeprom(object):

    def __init__(self, client):
        self.client = client

    @command('EEPROM','I')
    def read(self, addr):
        return self.client.recv_uint32()

    @command('EEPROM')
    def write_enable(self):
        pass

    @command('EEPROM','I')
    def erase(self, addr):
        pass

    @command('EEPROM','II')
    def write(self, addr, data_in):
        pass

    @command('EEPROM')
    def erase_all(self):
        pass

    @command('EEPROM','I')
    def write_all(self, data_in):
        pass

    @command('EEPROM')
    def erase_write_disable(self):
        pass

    def test(self, verbose=True):
        if verbose:
            print('Testing EEPROM driver')
        self.write_enable()
        addr = 12
        val = 42
        for i in range(10):
            self.write(addr, i)
            time.sleep(0.002)
            assert(self.read(addr) == i)
