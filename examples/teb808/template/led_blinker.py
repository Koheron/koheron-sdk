#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
from koheron import command, connect
import matplotlib.pyplot as plt
import numpy as np
import sys

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

if __name__=="__main__":
    host = os.getenv('HOST','10.211.3.40')
    client = connect(host, name='trenz_teb080x_te803_template')
    driver = LedBlinker(client)

    print ("Printing DNA: {}".format(driver.get_forty_two()))


