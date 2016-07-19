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
        time.sleep(0.01)
        eeprom.write(TEST_EEPROM_ADDR, 68)
        time.sleep(0.01)
        assert eeprom.read(TEST_EEPROM_ADDR) == 68

    def test_erase(self):
        eeprom.write_enable()
        time.sleep(0.01)
        eeprom.write(TEST_EEPROM_ADDR, 74)
        time.sleep(0.01)
        assert eeprom.read(TEST_EEPROM_ADDR) == 74
        eeprom.erase(TEST_EEPROM_ADDR)
        time.sleep(0.01)
        assert eeprom.read(TEST_EEPROM_ADDR) == 65535

    def test_erase_all(self):
        eeprom.write_enable()
        time.sleep(0.01)
        eeprom.write(TEST_EEPROM_ADDR, 74)
        time.sleep(0.01)
        assert eeprom.read(TEST_EEPROM_ADDR) == 74
        eeprom.erase_all()
        time.sleep(0.01)
        assert eeprom.read(TEST_EEPROM_ADDR) == 65535

    def test_write_all(self):
        eeprom.write_enable()
        eeprom.write_all(42)
        time.sleep(0.01)
        assert eeprom.read(TEST_EEPROM_ADDR) == 42
