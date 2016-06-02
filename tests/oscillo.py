import context
import os
from koheron_tcp_client import KClient, command

from drivers.common import Common
from drivers.oscillo import Oscillo

host = os.getenv('HOST','192.168.1.100')

client = KClient(host)

class Test:

    def __init__(self, client):
        self.client = client
        self.common = Common(client)
        self.oscillo = Oscillo(client)

driver = Test(client)

driver.common.status()

driver.oscillo.reset_acquisition()

print driver.oscillo.get_adc()