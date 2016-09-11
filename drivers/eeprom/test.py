import os
import time

from koheron import load_instrument
from eeprom import Eeprom

host = os.getenv('HOST','192.168.1.100')
instrument = os.getenv('NAME','')
client = load_instrument(host, instrument)

eeprom = Eeprom(client)

TEST_EEPROM_ADDR = 63

class TestsEeprom:
    def test_write_read(self):
        eeprom.write_enable()
        time.sleep(0.01)
        eeprom.write(TEST_EEPROM_ADDR, 68)
        time.sleep(0.01)
        assert eeprom.read(TEST_EEPROM_ADDR) == 68

    def test_write_read_many(self):
        eeprom.write_enable()
        for i in range(10):
            eeprom.write(i, i + 4)
            time.sleep(0.01)
            assert eeprom.read(i) == i + 4

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
