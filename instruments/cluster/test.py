#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os, time
from koheron import command, connect

class Cluster(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_pulse_generator(self, pulse_width, pulse_period):
        pass

    @command()
    def phase_shift(self, incdec):
        pass

    @command()
    def trig_pulse(self):
        pass

    def set_clk_source(self, source):
        if source == 'crystal':
            self.clk_sel(0)
        elif source == 'sata':
            self.clk_sel(1)

    @command()
    def clk_sel(self, clk_sel):
        pass

    @command()
    def ctl_sata(self, sata_out_sel, trig_delay):
        pass

    @command()
    def read_sata(self):
        return self.client.recv_uint32()

    @command()
    def set_freq(self, freq):
        pass

    @command()
    def get_adc(self):
        return self.client.recv_tuple('ii')

if __name__=="__main__":
    # Define the IP addresses of the 4 Red Pitayas
    hosts = ['192.168.1.14', '192.168.1.5', '192.168.1.13', '192.168.1.6']
    clients = []
    drivers = []

    for i, host in enumerate(hosts):
        clients.append(connect(host, 'cluster', restart=False))
        drivers.append(Cluster(clients[i]))

    for driver in drivers:
        driver.set_freq(10e6)
        driver.set_clk_source('crystal')
        driver.ctl_sata(1, 0)
        driver.set_pulse_generator(100, 200)

    for i in [1,2,3]:
        drivers[i].set_clk_source('sata')

    drivers[0].ctl_sata(1, 0)
    drivers[1].ctl_sata(0, 7)
    drivers[2].ctl_sata(0, 4)
    drivers[3].ctl_sata(0, 2)

    for i in range(10000):
        drivers[1].phase_shift(1)
        print(i)
        time.sleep(0.01)


