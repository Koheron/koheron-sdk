#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import connect
import os
import time
from led_blinker import LedBlinker

host = os.getenv('HOST','192.168.1.100')
client = connect(host, name='led-blinker')
driver = LedBlinker(client)

print(driver.get_forty_two())

print('Start blinking...')
for i in range(255):
    driver.set_leds(i)
    time.sleep(0.01)