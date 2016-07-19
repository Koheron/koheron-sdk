import context
import os
import time
import numpy as np

from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient

from drivers.laser import Laser

host = os.getenv('HOST','192.168.1.2')
project = os.getenv('NAME', '')

im = InstrumentManager(host)
im.install_instrument(project, always_restart=False)

client = KClient(host)
laser = Laser(client)

class TestsLaser:
    def test_is_laser_present(self):
        assert laser.is_laser_present()

    def test_laser_current(self):
        laser.start_laser()
        curr_setpt = 26.3 # mA
        laser.set_laser_current(curr_setpt)
        rel_err = abs(curr_setpt * 1E-3 / laser.get_laser_current() - 1)
        assert rel_err < 0.1

    def test_laser_power(self):
        laser.start_laser()
        laser.set_laser_current(0.0)
        time.sleep(0.1)
        power_off = laser.get_laser_power()
        laser.set_laser_current(30.0)
        time.sleep(0.1)
        power_on = laser.get_laser_power()
        assert power_off < power_on
