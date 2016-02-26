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
        self.open_gpio()

    def open_gpio(self):
        @command('GPIO')
        def open(self): pass

        open(self)

    @command('GPIO')
    def set_bit(self, index, channel): pass

    @command('GPIO')
    def clear_bit(self, index, channel): pass

    @command('GPIO')
    def toggle_bit(self, index, channel): pass

    @command('GPIO')
    def set_as_input(self, index, channel): pass

    @command('GPIO')
    def set_as_output(self, index, channel): pass