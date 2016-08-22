# -*- coding: utf-8 -*-

from koheron_tcp_client import command

class Xadc(object):

    def __init__(self, client):
        self.client = client
        self.enable_averaging()
        self.set_averaging(256)

    @command('Xadc','II')
    def set_channel(self, channel_0, channel_1):
        return self.client.recv_int32()

    @command('Xadc')
    def enable_averaging(self): pass

    @command('Xadc','I')
    def set_averaging(self, n_avg):
        return self.client.recv_int32()

    @command('Xadc','I')
    def read(self, channel):
        return self.client.recv_int32()

    def status(self):
        for channel in [1,8]:
            print('XADC {} = {} arb. units'.format(channel, self.read(channel)))
