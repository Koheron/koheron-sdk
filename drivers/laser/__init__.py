# -*- coding: utf-8 -*-

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

    @command('LASER','f')
    def set_laser_current(self, current):
        """ current: The bias in mA """
        pass

    @command('LASER')
    def is_laser_present(self):
        return self.client.recv_uint32()

    @command('LASER')
    def save_config(self):
        pass

    @command('LASER')
    def load_config(self):
        return self.client.recv_uint32()

    def status(self):
        print('laser current = {} mA'.format(self.get_laser_current()))
        print('laser power = {} arb. units'.format(self.get_laser_power()))

