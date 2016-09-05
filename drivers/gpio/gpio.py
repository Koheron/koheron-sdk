# -*- coding: utf-8 -*-

from koheron import command

class Gpio(object):

    def __init__(self, client):
        self.client = client

    @command()
    def set_data(self, channel, value): pass

    @command()
    def get_data(self, channel):
        return self.client.recv_uint32()

    @command()
    def set_bit(self, index, channel): pass

    @command()
    def clear_bit(self, index, channel): pass

    @command()
    def toggle_bit(self, index, channel): pass

    @command()
    def read_bit(self, index, channel):
        return self.client.recv_bool()

    @command()
    def set_as_input(self, index, channel): pass

    @command()
    def set_as_output(self, index, channel): pass
