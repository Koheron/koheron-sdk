import os
import time
from koheron import connect, command

class ClockGenerator(object):
    def __init__(self, client):
        self.client = client

    @command()
    def set_tcxo_clock(self, val):
        pass

    @command()
    def set_sampling_frequency(self, val):
        pass

    @command()
    def set_reference_clock(self, val):
        pass

    @command()
    def set_tcxo_clock(self, val):
        return self.client.recv_int32()

    @command()
    def set_tcxo_calibration(self, val):
        return self.client.recv_int32()

host = os.getenv('HOST','192.168.1.22')
driver = ClockGenerator(connect(host, 'fft', restart=False))

# ------------------------------------------------------------------------------------
# Stress test: 
#   Test the probability for the CPU to stall by alternating
#   rapidly the sampling frequency between 200 and 250 MHz.
# ------------------------------------------------------------------------------------

for i in range(10000000):
    print(i)
    time.sleep(0.0001)
    print(driver.set_sampling_frequency(i % 2))