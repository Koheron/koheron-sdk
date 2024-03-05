#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np
import os
from koheron import command, connect

class TestClock(object):
    def __init__(self, client):
        self.client = client

    @command()
    def get_counter_fclk0(self):
        return self.client.recv_uint64()

    @command()
    def get_counter_adc_clk(self):
        return self.client.recv_uint64()


host = os.getenv('HOST', '192.168.1.114')
client = connect(host, 'test-clock', restart=True)
driver = TestClock(client)

adc_clk_MHz = 250.0

while True:
    fclk0_Mhz = adc_clk_MHz * float(driver.get_counter_fclk0()) / float(driver.get_counter_adc_clk())
    print("fclk0 frequency = {} MHz".format(fclk0_Mhz))
