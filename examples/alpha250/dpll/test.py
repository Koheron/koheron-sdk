#!/usr/bin/env python3

import os
from koheron import command, connect

class Dpll:
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
    host = os.getenv('HOST', '192.168.1.100')
    client = connect(host, name='dpll')
    driver = Dpll(client)

    driver.set_reference_clock(0)
    # Keep the sampling frequency selected by the instrument.
