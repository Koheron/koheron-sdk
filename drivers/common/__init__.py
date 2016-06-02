#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron_tcp_client import command

class Common(object):

    def __init__(self, client):
        self.client = client
        if self.open_common() < 0:
            print('Cannot open COMMON device')

    def open_common(self):
        @command('COMMON')
        def open(self):
            return self.client.recv_int(4)

        return open(self)

    @command('COMMON')
    def get_bitstream_id(self):
        id_array = self.client.recv_buffer(8, data_type='uint32')
        return ''.join('{:08x}'.format(i) for i in id_array)

    @command('COMMON')
    def get_dna(self):
        buff = self.client.recv_buffer(2, data_type='uint32')
        return buff[1] + buff[0] << 32

    @command('COMMON')
    def set_led(self, value): pass

    @command('COMMON')
    def get_led(self): 
        return self.client.recv_int(4)

    @command('COMMON')
    def ip_on_leds(self): pass

    def status(self):
       print('bitstream id = {}'.format(self.get_bitstream_id()))
       print('DNA = {}'.format(self.get_dna()))
       print('LED = {}'.format(self.get_led() % 256))