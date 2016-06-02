import context
import os
from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient, command

from drivers.common import Common
from drivers.spectrum import Spectrum
from drivers.laser import Laser
from drivers.xadc import Xadc
from drivers.gpio import Gpio

host = os.getenv('HOST','192.168.1.100')
im = InstrumentManager(host)
im.install_instrument('spectrum')

client = KClient(host)

class Test:

    def __init__(self, client):
        self.client = client

        self.common = Common(client)
        self.spectrum = Spectrum(client)
        self.xadc = Xadc(client)
        self.laser = Laser(client)
        self.gpio = Gpio(client)

driver = Test(client)

driver.common.status()
driver.xadc.status()

driver.laser.set_laser_current(30)
driver.laser.status()

driver.spectrum.reset_acquisition()

print driver.spectrum.get_spectrum()
print driver.spectrum.get_num_average()

print driver.laser.get_monitoring()