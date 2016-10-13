import os
import numpy as np

from koheron import connect
from gpio import Gpio

host = os.getenv('HOST','192.168.1.100')
instrument = os.getenv('NAME','')
client = connect(host, name=instrument)

gpio = Gpio(client)

class TestsGpio:
    def test_set_get_data(self):
        for i in range(8):
            gpio.set_as_output(i, 0)

        gpio.set_data(0, 7)
        assert gpio.get_data(0) == 7

    def test_set_bit(self):
        gpio.set_bit(2, 1)
        assert gpio.read_bit(2, 1)

    def test_clear_bit(self):
        gpio.clear_bit(4, 0)
        assert not gpio.read_bit(4, 0)

    def test_toggle_bit(self):
        val = gpio.read_bit(1, 0)
        gpio.toggle_bit(1, 0)
        assert gpio.read_bit(1, 0) == (not val)

tests = TestsGpio()
tests.test_set_get_data()
