#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron_tcp_client import command, write_buffer

class Common(object):
    """ Common commands for all bitstreams.

    args:
        client : instance of KClient connected to tcp-server.
    """
    def __init__(self, wfm_size, client):
        self.client = client
        self.open_common()

    def open_common(self, wfm_size):
        @command('COMMON')
        def open(self): pass

        open(self)

    @command('COMMON')
    def get_bitstream_id(self):
        id_array = self.client.recv_buffer(8, data_type='uint32')
        return ''.join('{:08x}'.format(i) for i in id_array)

    @command('COMMON')
    def get_dna(self): 
        return self.client.recv_int(8)

    @command('COMMON')
    def set_led(self, value): pass

    @command('COMMON')
    def get_led(self): 
        return self.client.recv_int(4)

    @command('COMMON')
    def ip_on_leds(self): pass