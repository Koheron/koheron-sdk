#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron_tcp_client import command

class Blink(object):
    def __init__(self, client):
        self.client = client
        self.open_blink()

    def open_blink(self):
        @command('COMMON')
        def open(self):
            pass

        open(self)

    @command('COMMON')
    def get_bitstream_id(self):
        return self.client.recv_buffer(8, data_type='uint32')

