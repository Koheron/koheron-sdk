# -*- coding: utf-8 -*-
import time
import numpy as np

from koheron_tcp_client import command, write_buffer

class Eeprom(object):

    def __init__(self, client):
        self.client = client

    @command('Eeprom','I')
    def read(self, addr):
        return self.client.recv_uint32()

    @command('Eeprom')
    def write_enable(self):
        pass

    @command('Eeprom','I')
    def erase(self, addr):
        pass

    @command('Eeprom','II')
    def write(self, addr, data_in):
        pass

    @command('Eeprom')
    def erase_all(self):
        pass

    @command('Eeprom','I')
    def write_all(self, data_in):
        pass

    @command('Eeprom')
    def erase_write_disable(self):
        pass
