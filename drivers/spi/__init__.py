
from koheron import command

class Spi(object):
    def __init__(self, client):
        self.client = client

    @command()
    def write(self, data):
        return self.client.recv_int32()
