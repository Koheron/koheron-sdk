# -*- coding: utf-8 -*-

import time
import numpy as np
from koheron import command

class Laser(object):

    def __init__(self, client):
        self.client = client

    @command('LASER')
    def is_laser_present(self):
        return self.client.recv_bool()

    @command('LASER')
    def reset(self): pass

    @command('LASER')
    def start_laser(self): pass

    @command('LASER')
    def stop_laser(self): pass

    @command('LASER')
    def get_laser_current(self):
        current = self.client.recv_uint32()
        return (0.0001/21.) * current

    @command('LASER')
    def get_laser_power(self):
        return self.client.recv_uint32()

    @command('LASER')
    def get_monitoring(self):
        return self.client.recv_tuple('II')

    @command('LASER')
    def get_status(self):
        return self.client.recv_tuple('?ff')

    @command('LASER','f')
    def set_laser_current(self, current):
        """ current: The bias in mA """
        pass

    @command('LASER')
    def save_config(self): pass

    @command('LASER')
    def load_config(self):
        return self.client.recv_float()

    def status(self):
        print('laser current = {} mA'.format(self.get_laser_current()))
        print('laser power = {} arb. units'.format(self.get_laser_power()))
