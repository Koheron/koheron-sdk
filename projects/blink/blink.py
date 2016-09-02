#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
from koheron import command, load_instrument

class Blink(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_dac(self, dac0, dac1):
        pass

    @command()
    def get_adc(self):
        return self.recv_tuple('II')

if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.100')
    client = load_instrument(host, 'blink')
    driver = Blink(client)

    print driver.get_adc()


   


