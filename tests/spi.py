import context
import os
from koheron_tcp_client import KClient

from drivers.spi import Spi

host = os.getenv('HOST','192.168.1.2')

client = KClient(host)
spi = Spi(client)