import context
import os
import numpy as np

from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient

from drivers.gpio import Gpio

host = os.getenv('HOST','192.168.1.2')
project = os.getenv('NAME', '')

im = InstrumentManager(host)
im.install_instrument(project, always_restart=False)

client = KClient(host)
gpio = Gpio(client)

class TestsGpio:
    def test_set_get_data(self):
        for i in range(8):
            gpio.set_as_output(i, 0)

        gpio.set_data(0, 7)
        assert gpio.get_data(0) == 7

tests = TestsGpio()
tests.test_set_get_data()