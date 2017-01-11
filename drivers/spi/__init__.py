
from koheron import command, connect

class Spi(object):
    def __init__(self, client):
        self.client = client

    @command()
    def write(self, data):
        return self.client.recv_int32()

if __name__ == "__main__":
    import os
    import numpy as np

    host = os.getenv('HOST','192.168.1.100')
    client = connect(host, name='oscillo')
    spi = Spi(client)
    spi.write(np.array([1235], dtype='uint32'))