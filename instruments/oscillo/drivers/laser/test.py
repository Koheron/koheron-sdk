import os
import time
import numpy as np

from koheron import connect
from laser import Laser

host = os.getenv('HOST','192.168.1.100')
instrument = os.getenv('NAME','')
client = connect(host, name=instrument)

laser = Laser(client)

class TestsLaser:
    def test_is_laser_present(self):
        assert laser.is_laser_present()

    def test_laser_current(self):
        laser.start_laser()
        curr_setpt = 26.3 # mA
        laser.set_laser_current(curr_setpt)
        time.sleep(0.1)
        rel_err = abs(curr_setpt * 1E-3 / laser.get_laser_current() - 1)
        assert rel_err < 0.1

    def test_laser_power(self):
        laser.start_laser()
        laser.set_laser_current(0.0)
        time.sleep(0.1)
        power_off = laser.get_laser_power()
        assert power_off < 1000
        laser.set_laser_current(30.0)
        time.sleep(0.1)
        power_on = laser.get_laser_power()
        assert power_off < power_on

    def test_config(self):
        curr_setpt = 10.32 # mA
        laser.set_laser_current(curr_setpt)
        laser.save_config()
        time.sleep(0.1)
        rel_err = abs(curr_setpt * 1E-3 / laser.load_config() - 1)
        assert rel_err < 5E-3

    def test_status(self):
        laser.stop_laser()
        assert not laser.get_status()[0]
        laser.start_laser()
        assert laser.get_status()[0]

tests = TestsLaser()
tests.test_status()
