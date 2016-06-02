import context
import os
from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient, command
from project_config import ProjectConfig

from drivers.common import Common
from drivers.oscillo import Oscillo
from drivers.laser import Laser
from drivers.xadc import Xadc
from drivers.gpio import Gpio
from drivers.device_memory import DeviceMemory

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

driver = Test(client)

driver.common.status()
driver.xadc.status()

driver.laser.set_laser_current(30)
driver.laser.status()

driver.oscillo.reset_acquisition()

print driver.oscillo.get_adc()
print driver.laser.get_monitoring()


# Test device memory
dvm = DeviceMemory(client)

for mmap in pc.mmaps:
    dvm.add_mmap(mmap)

value = 42
dvm.write32('config', pc.cfg['led'], value)
assert(dvm.read32('config', pc.cfg['led']) == value)

dna = dvm.read32('status', pc.sts['dna']) + dvm.read32('status', pc.sts['dna']+4) << 32
assert(driver.common.get_dna() == dna)



