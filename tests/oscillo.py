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
from drivers.eeprom import Eeprom

host = os.getenv('HOST','192.168.1.100')
project = os.getenv('NAME','')

im = InstrumentManager(host)
im.install_instrument(project)
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
        self.eeprom = Eeprom(client)
       
driver = Test(client)

driver.common.status()
driver.xadc.status()

driver.laser.test()
driver.oscillo.test()
#driver.eeprom.test()

# Test device memory
dvm = DeviceMemory(client)

for mmap in pc.mmaps:
    dvm.add_mmap(mmap)

value = 42
dvm.write32('config', pc.cfg['led'], value)
assert(dvm.read32('config', pc.cfg['led']) == value)

for i in range(10):
    dvm.write_buffer('dac1', 0, 1024 *  np.ones(4096))
