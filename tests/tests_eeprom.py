import context
import os
import time
import numpy as np

from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient

from drivers.eeprom import Eeprom

host = os.getenv('HOST','192.168.1.2')
project = os.getenv('NAME', '')

im = InstrumentManager(host)
im.install_instrument(project, always_restart=False)

client = KClient(host)
eeprom = Eeprom(client)

TEST_EEPROM_ADDR = 63

class TestsEeprom:
    def test_write_read(self):
        eeprom.write_enable()
        time.sleep(0.1)
        eeprom.write(TEST_EEPROM_ADDR, 68)
        time.sleep(0.1)
        assert eeprom.read(TEST_EEPROM_ADDR) == 68