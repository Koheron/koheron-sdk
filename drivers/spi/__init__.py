
from koheron import command

class Spi(object):
    def __init__(self, client):
        self.client = client

        @command()
        def init(self, mode):
            return self.client.recv_int32()

        if init(self, 0) < 0:
            raise RuntimeError('Cannot open SPI device')

    @command()
    def write(self, data):
        return self.client.recv_int32()
