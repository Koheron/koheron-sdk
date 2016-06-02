from config import *

import os
from koheron_tcp_client import KClient, command

host = os.getenv('HOST','192.168.1.100')

client = KClient(host)

class Test:

    def __init__(self, client):
        self.client = client
        if self.open_common() < 0:
            print('Cannot open COMMON device')

    def open_common(self):
        @command('COMMON')
        def open(self):
            return self.client.recv_int(4)

        return open(self)

    @command('COMMON')
    def get_bitstream_id(self):
        id_array = self.client.recv_buffer(8, data_type='uint32')
        return ''.join('{:08x}'.format(i) for i in id_array)

driver = Test(client)

print('bitstream id = ' + driver.get_bitstream_id())