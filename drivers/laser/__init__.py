# -*- coding: utf-8 -*-

import time
import numpy as np
from koheron import command

class Laser(object):

    def __init__(self, client):
        self.client = client

    @command()
    def is_laser_present(self):
        return self.client.recv_bool()

    @command()
    def reset(self): pass

    @command()
    def start_laser(self): pass

    @command()
    def stop_laser(self): pass

    @command()
    def get_laser_current(self):
        current = self.client.recv_uint32()
        return (0.0001/21.) * current

    @command()
    def get_laser_power(self):
        return self.client.recv_uint32()

    @command()
    def get_monitoring(self):
        return self.client.recv_tuple('II')

    @command()
    def get_status(self):
        return self.client.recv_tuple('?ff')

    @command()
    def set_laser_current(self, current):
        """ current: The bias in mA """
        pass

    @command()
    def save_config(self): pass

    @command()
    def load_config(self):
        return self.client.recv_float()

    def status(self):
        print('laser current = {} mA'.format(self.get_laser_current()))
        print('laser power = {} arb. units'.format(self.get_laser_power()))
