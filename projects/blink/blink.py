#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import command

class Blink(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_dac(self, dac0, dac1):
        pass

    @command()
    def get_adc(self):
        return self.client.recv_tuple('II')
   


