import context
import os
from koheron_tcp_client import KClient, command

from drivers.common import Common

host = os.getenv('HOST','192.168.1.100')

client = KClient(host)

class Test:

    def __init__(self, client):
        self.client = client
        self.common = Common(client)

driver = Test(client)

driver.common.print_status()
