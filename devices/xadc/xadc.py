#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron_tcp_client import command

class Xadc(object):
    """ XADC interface.

    args:
        client : instance of KClient connected to tcp-server.
    """
    def __init__(self, client):
        self.client = client
        self.open_xadc()

    def open_xadc(self):
        @command('XADC')
        def open(self): pass

        open(self)

    @command('XADC')
    def set_channel(self, channel_0, channel_1): pass

    @command('XADC')
    def enable_averaging(self): pass

    @command('XADC')
    def set_averaging(self, n_avg): pass

    @command('XADC')
    def read(self, channel):
        return self.client.recv_int(4)