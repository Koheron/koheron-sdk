#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron_tcp_client import command

class Laser(object):
    """ Basic control and monitoring for Koheron Laser Board."""
    def __init__(self, client):
        """ client : instance of KClient connected to tcp-server."""
        self.client = client
        self.open_laser()
        self.max_current = 40  # mA

    def open_laser(self):
        @command('LASER')
        def open(self): pass

        open(self)

    def close(self):
        self.reset()

    @command('LASER')
    def reset(self): pass

    @command('LASER')
    def start_laser(self): pass

    @command('LASER')
    def stop_laser(self): pass

    @command('LASER')
    def get_laser_current(self):
        current = self.client.recv_int(4)
        return (0.0001/21.) * current

    @command('LASER')
    def get_laser_power(self):
        return self.client.recv_int(4)

    @command('LASER')
    def get_monitoring(self):
        return self.client.recv_tuple()

    @command('LASER')
    def set_laser_current(self, current):
        """ current: The bias in mA """
        pass
