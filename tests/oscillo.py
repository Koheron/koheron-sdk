import context
import os
from koheron import load_instrument

from drivers.common import Common
from drivers.oscillo import Oscillo
from drivers.laser import Laser
from drivers.xadc import Xadc
from drivers.gpio import Gpio
from drivers.eeprom import Eeprom

host = os.getenv('HOST','192.168.1.100')
project = os.getenv('NAME','')
client = load_instrument(host, project)


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

