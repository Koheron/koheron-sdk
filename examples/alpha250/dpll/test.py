#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import time
from koheron import command, connect
import matplotlib.pyplot as plt
import numpy as np

class Dpll(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_integrator(self, channel, integrator_index, integrator_on):
        pass

    @command()
    def set_dds_freq(self, channel, freq):
        pass

    @command()
    def set_p_gain(self, channel, p_gain):
        pass

    @command()
    def set_pi_gain(self, channel, pi_gain):
        pass

    @command()
    def set_i2_gain(self, channel, i2_gain):
        pass

    @command()
    def set_i3_gain(self, channel, i3_gain):
        pass

    @command(classname="ClockGenerator")
    def set_tcxo_clock(self, val):
        pass


    @command(classname="ClockGenerator")
    def set_sampling_frequency(self, val):
        pass

    @command(classname="ClockGenerator")
    def set_reference_clock(self, val):
        pass

    @command(classname="ClockGenerator")
    def set_tcxo_clock(self, val):
        pass



if __name__=="__main__":
    host = os.getenv('HOST','192.168.1.20')
    client = connect(host, name='dpll')
    driver = Dpll(client)

    #driver.set_reference_clock(2)
    #driver.set_tcxo_clock(117)

    driver.set_reference_clock(0)
    driver.set_sampling_frequency(0)

    #driver.set_dds_freq(0, 30e6)

    #driver.set_p_gain(0, 0)
    #driver.set_pi_gain(0, 0)
    #driver.set_i2_gain(0, 0)
    #driver.set_i3_gain(0, 0)

    #driver.set_dds_freq(1, 30.00e6)
