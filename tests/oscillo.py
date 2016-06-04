import context
import os
from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient, command
from project_config import ProjectConfig
import time
import numpy as np

from drivers.common import Common
from drivers.oscillo import Oscillo
from drivers.laser import Laser
from drivers.xadc import Xadc
from drivers.gpio import Gpio
from drivers.device_memory import DeviceMemory
from drivers.at93c46d import At93c46d

host = os.getenv('HOST','192.168.1.100')
project = os.getenv('NAME','')

im = InstrumentManager(host)
#im.install_instrument(project)
client = KClient(host)

pc = ProjectConfig(project)

class Test:

    def __init__(self, client):
        self.client = client

        self.common = Common(client)
        self.oscillo = Oscillo(client)
        self.xadc = Xadc(client)
        self.laser = Laser(client)
        self.gpio = Gpio(client)
        self.eeprom = At93c46d(client)

    def test_laser(self, verbose=True):
        laser = self.laser
        assert(laser.is_laser_present())
        laser.start_laser()
        if verbose:
            print('Loading laser configuration, current = {} mA'.format(laser.load_config()))
        laser.set_laser_current(30)
        time.sleep(0.01)
        driver.laser.status()
        driver.laser.save_config()

    def test_oscillo(self, verbose=True):
        oscillo = self.oscillo
        oscillo.reset()
        time.sleep(0.01)
        adc = oscillo.get_adc()
        assert(np.shape(adc) == (2, 8192))
        mean = np.mean(adc)
        std = np.std(adc)
        if verbose:
            print('Get adc, mean = {}, std = {}'.format(mean, std))
        assert(std > 0)

    def test_eeprom(self):
        eeprom = self.eeprom
        addr = 12
        val = 42
        for i in range(10):
            driver.eeprom.write(addr, i)
            time.sleep(0.002)
            assert(driver.eeprom.read(addr) == i)


driver = Test(client)

driver.common.status()
driver.xadc.status()

driver.test_laser()
driver.test_oscillo()
driver.test_eeprom()


# Test EEPROM

#driver.eeprom.erase_write_disable()



# # Test device memory
# dvm = DeviceMemory(client)

# for mmap in pc.mmaps:
#     dvm.add_mmap(mmap)

# value = 42
# dvm.write32('config', pc.cfg['led'], value)
# assert(dvm.read32('config', pc.cfg['led']) == value)

# dna = dvm.read32('status', pc.sts['dna']) + dvm.read32('status', pc.sts['dna']+4) << 32
# assert(driver.common.get_dna() == dna)
