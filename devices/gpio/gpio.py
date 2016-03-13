#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron_tcp_client import command

class Gpio(object):
    """ GPIO control interface.

    args:
        client : instance of KClient connected to tcp-server.
    """
    def __init__(self, client):
        self.client = client
        if self.open_gpio() < 0:
            print('Cannot open GPIO device')

    def open_gpio(self):
        @command('GPIO')
        def open(self):
            return self.client.recv_int(4)

        return open(self)

    @command('GPIO')
    def set_bit(self, index, channel):
        return self.client.recv_int(4)

    @command('GPIO')
    def clear_bit(self, index, channel):
        return self.client.recv_int(4)

    @command('GPIO')
    def toggle_bit(self, index, channel):
        return self.client.recv_int(4)

    @command('GPIO')
    def set_as_input(self, index, channel):
        return self.client.recv_int(4)

    @command('GPIO')
    def set_as_output(self, index, channel):
        return self.client.recv_int(4)