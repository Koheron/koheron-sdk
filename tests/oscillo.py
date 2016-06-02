import context
import os
from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient, command

from drivers.common import Common
from drivers.oscillo import Oscillo
from drivers.laser import Laser
from drivers.xadc import Xadc
from drivers.gpio import Gpio
from drivers.device_memory import DeviceMemory

host = os.getenv('HOST','192.168.1.100')
im = InstrumentManager(host)
im.install_instrument('oscillo')
client = KClient(host)

class Test:

    def __init__(self, client):
        self.client = client

        self.common = Common(client)
        self.oscillo = Oscillo(client)
        self.xadc = Xadc(client)
        self.laser = Laser(client)
        self.gpio = Gpio(client)
        self.dvm = DeviceMemory(client)

driver = Test(client)

driver.common.status()
driver.xadc.status()

driver.laser.set_laser_current(30)
driver.laser.status()

driver.oscillo.reset_acquisition()

print driver.oscillo.get_adc()
print driver.laser.get_monitoring()


# Test device memory

driver.dvm.add_map('config', '0x60000000', '4K')
value = 42
driver.dvm.write32('config', 0, value)
assert(driver.dvm.read32('config', 0) == value)