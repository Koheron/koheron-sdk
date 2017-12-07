#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import command

class LedBlinker(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_leds(self, led_value):
        pass

    @command()
    def get_leds(self, led_value):
        return self.client.recv_uint32()

    @command()
    def set_led(self, index, status):
        pass

    @command()
    def get_forty_two(self):
        return self.client.recv_uint32()