import os
import time
from koheron import connect, command

class VCO(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_dds_freq(self, channel, freq):
        pass

    @command()
    def set_vco_gain(self, channel, gain):
        pass

    @command()
    def get_adc0(self):
        return self.client.recv_uint32()

    @command(classname="ClockGenerator")
    def set_reference_clock(self, val):
        pass


host = os.getenv('HOST','192.168.1.22')
driver = VCO(connect(host, 'vco', restart=False))

driver.set_reference_clock(0)
driver.set_dds_freq(0, 10e6)
driver.set_vco_gain(0,4)
