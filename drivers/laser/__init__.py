# -*- coding: utf-8 -*-

import time
import numpy as np
from koheron_tcp_client import command

class Laser(object):

    def __init__(self, client):
        self.client = client
        if self.open() < 0 :
            print('Cannot open LASER device')

    @command('LASER')
    def open(self):
        return self.client.recv_int32()

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
        return self.client.recv_tuple()

    @command('LASER')
    def get_status(self):
        return self.client.recv_tuple()

    @command('LASER','f')
    def set_laser_current(self, current):
        """ current: The bias in mA """
        pass

    @command('LASER')
    def save_config(self): pass

    @command('LASER')
    def load_config(self):
        return self.client.recv_int(4, fmt='f')

    def status(self):
        print('laser current = {} mA'.format(self.get_laser_current()))
        print('laser power = {} arb. units'.format(self.get_laser_power()))

    def test(self):
        print('Testing LASER driver')
        current_setpoint = 0.03
        if not self.is_laser_present():
            print('Laser is not present')
            return
        self.start_laser()
        self.set_laser_current(1000 * current_setpoint)
        self.save_config()
        time.sleep(0.01)
        self.set_laser_current(0)
        config_current = self.load_config()
        assert(np.abs(config_current - current_setpoint) < 0.01) # Check that current is set to 30 mA
        print('Loading laser configuration, current = {:.2f} mA'.format(1000 * config_current))
        time.sleep(0.01)
        measured_current = self.get_laser_current()
        relative_error = measured_current / config_current - 1
        assert(np.abs(relative_error) < 0.1)

