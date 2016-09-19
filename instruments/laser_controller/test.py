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
        return self.client.recv_uint32()

    @command()
    def get_current(self):
        return self.client.recv_uint32()

    @command()
    def set_current(self, current):
    	''' Laser current in mA '''
        pass

if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.100')
    client = connect(host, 'laser_controller')
    laser = Laser(client)

    laser.start()
    for i in range(40):
        laser.set_current(i)
        print(laser.get_current, laser.get_power())