
from koheron import command, write_buffer

class Spi(object):
    def __init__(self, client):
        self.client = client

        @command('SPI', 'I')
        def init(self, mode):
            return self.client.recv_int32()

        if init(self, 0) < 0:
            raise RuntimeError('Cannot open SPI device')

    @write_buffer('SPI')
    def write(self, data):
        return self.client.recv_int32()
