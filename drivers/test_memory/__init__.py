# -*- coding: utf-8 -*-

from koheron import command

class TestMemory(object):

    def __init__(self, client):
        self.client = client

    @command('TestMemory')
    def write_read_u32(self):
        return self.client.recv_bool()

    @command('TestMemory')
    def write_read_i16(self):
        return self.client.recv_bool()

    @command('TestMemory')
    def write_read_float(self):
        return self.client.recv_bool()
