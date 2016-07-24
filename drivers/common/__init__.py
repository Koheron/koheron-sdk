#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron_tcp_client import command

class Common(object):

    def __init__(self, client):
        self.client = client

    @command('COMMON')
    def get_bitstream_id(self):
        id_array = self.client.recv_buffer(8, data_type='uint32')
        return ''.join('{:08x}'.format(i) for i in id_array)

    @command('COMMON')
    def get_dna(self):
        id_array = self.client.recv_buffer(2, data_type='uint32')
        return ''.join('{:02x}'.format(i) for i in id_array)

    @command('COMMON', 'I')
    def set_led(self, value): pass

    @command('COMMON')
    def get_led(self):
        return self.client.recv_uint32()

    @command('COMMON')
    def init(self): pass

    @command('COMMON')
    def ip_on_leds(self): pass

    def get_server_version(self):
        @command('KSERVER')
        def get_version(self):
            return self.client.recv_string()

        return get_version(self)

    def status(self):
       print('bitstream id = {}'.format(self.get_bitstream_id()))
       print('DNA = {}'.format(self.get_dna()))
       print('LED = {}'.format(self.get_led() % 256))
