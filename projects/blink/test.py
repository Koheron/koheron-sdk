#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
from koheron import load_instrument
from blink import Blink

if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.100')
    client = load_instrument(host, 'blink')
    driver = Blink(client)

    driver.set_dac(0,1000)
    adc1, adc2 = driver.get_adc()
    print('adc1 = {}, adc2 = {}'.format(adc1, adc2))


   


