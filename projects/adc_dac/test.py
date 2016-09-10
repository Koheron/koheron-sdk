#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
from koheron import command, load_instrument

class AdcDac(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_dac(self, dac0, dac1):
        pass

    @command()
    def get_adc(self):
        return self.client.recv_tuple('II')

if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.100')
    client = load_instrument(host, 'adc_dac')
    driver = AdcDac(client)

    driver.set_dac(0,1000)
    adc1, adc2 = driver.get_adc()
    print('adc1 = {}, adc2 = {}'.format(adc1, adc2))


   


