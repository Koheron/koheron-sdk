import context
import os
import numpy as np
from koheron_tcp_client import KClient

from drivers.spi import Spi

host = os.getenv('HOST','192.168.1.2')

client = KClient(host)
spi = Spi(client)

# data = np.arange(50)
data = np.ones(200) * 2**31
print spi.write(data)