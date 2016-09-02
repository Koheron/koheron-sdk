# -*- coding: utf-8 -*-
import time
import numpy as np

from koheron import command

class Eeprom(object):

    def __init__(self, client):
        self.client = client

    @command()
    def read(self, addr):
        return self.client.recv_uint32()

    @command()
    def write_enable(self):
        pass

    @command()
    def erase(self, addr):
        pass

    @command()
    def write(self, addr, data_in):
        pass

    @command()
    def erase_all(self):
        pass

    @command()
    def write_all(self, data_in):
        pass

    @command()
    def erase_write_disable(self):
        pass
