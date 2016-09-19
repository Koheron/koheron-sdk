#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import time
from koheron import command, connect

class Laser(object):
    def __init__(self, client):
        self.client = client

    @command()
    def start(self):
        pass

    @command()
    def stop(self):
        pass

    @command()
    def get_power(self):
        return self.recv_uint32()

    @command()
    def get_current(self):
        return self.recv_uint32()

    @command()
    def set_current(self, current):
    	''' Laser current in mA '''
        pass

if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.100')
    client = connect(host, 'led_blinker')
    driver = Laser(client)

    laser.start()
    laser.set_current(30)
    print(laser.get_power())