# -*- coding: utf-8 -*-

from koheron_tcp_client import command

class Gpio(object):

    def __init__(self, client):
        self.client = client
        if self.open() < 0:
            print('Cannot open GPIO device')

    @command('GPIO')
    def open(self):
        return self.client.recv_int32()

    @command('GPIO', 'II')
    def set_data(self, channel, value): pass

    @command('GPIO', 'I')
    def get_data(self, channel):
        return self.client.recv_uint32()

    @command('GPIO', 'II')
    def set_bit(self, index, channel):
        return self.client.recv_int32()

    @command('GPIO', 'II')
    def clear_bit(self, index, channel):
        return self.client.recv_int32()

    @command('GPIO', 'II')
    def toggle_bit(self, index, channel):
        return self.client.recv_int32()

    @command('GPIO', 'II')
    def set_as_input(self, index, channel):
        return self.client.recv_int32()

    @command('GPIO', 'II')
    def set_as_output(self, index, channel):
        return self.client.recv_int32()
