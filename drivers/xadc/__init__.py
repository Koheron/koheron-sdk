# -*- coding: utf-8 -*-

from koheron_tcp_client import command

class Xadc(object):

    def __init__(self, client):
        self.client = client
        if self.open() < 0:
            print('Cannot open XADC device')

        print self.set_channel(1,8)
        self.enable_averaging()
        self.set_averaging(256)

    @command('XADC')
    def open(self):
        return self.client.recv_int32()

    @command('XADC','II')
    def set_channel(self, channel_0, channel_1):
        return self.client.recv_int32()

    @command('XADC')
    def enable_averaging(self): pass

    @command('XADC','I')
    def set_averaging(self, n_avg):
        return self.client.recv_int32()

    @command('XADC','I')
    def read(self, channel):
        return self.client.recv_int32()

    def status(self):
        for channel in [1,8]:
            print('XADC {} = {} arb. units'.format(channel, self.read(channel)))
