#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import time
from koheron import command, connect

class LedBlinker(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_led(self, led_value):
        pass

    @command()
    def get_forty_two(self):
        return self.client.recv_uint32()

if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.100')
    client = connect(host, 'led_blinker')
    driver = LedBlinker(client)

    print(driver.get_forty_two())

    print('Start blinking...')
    for i in range(255):
        driver.set_led(i)
        time.sleep(0.01)
