
from koheron_tcp_client import command, write_buffer

class Spi(object):

    def __init__(self, client):
        self.client = client

        @command('SPI', 'I')
        def init(self, mode):
            return self.client.recv_int32()

        if init(self, 0) < 0:
            print('Cannot open SPI device')

