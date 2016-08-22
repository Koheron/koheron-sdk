# -*- coding: utf-8 -*-

from koheron import command

class Gpio(object):

    def __init__(self, client):
        self.client = client

    @command('Gpio', 'II')
    def set_data(self, channel, value): pass

    @command('Gpio', 'I')
    def get_data(self, channel):
        return self.client.recv_uint32()

    @command('Gpio', 'II')
    def set_bit(self, index, channel): pass

    @command('Gpio', 'II')
    def clear_bit(self, index, channel): pass

    @command('Gpio', 'II')
    def toggle_bit(self, index, channel): pass

    @command('Gpio', 'II')
    def read_bit(self, index, channel):
        return self.client.recv_bool()

    @command('Gpio', 'II')
    def set_as_input(self, index, channel): pass

    @command('Gpio', 'II')
    def set_as_output(self, index, channel): pass
