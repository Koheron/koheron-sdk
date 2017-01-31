
from koheron import command, connect

class TestSpi(object):
    def __init__(self, client):
        self.client = client

    @command()
    def write(self, data):
        return self.client.recv_int32()

    @command()
    def read(self, n_pts):
        return self.client.recv_vector(dtype='uint16', check_type=False)

    @command()
    def read_array(self):
        return self.client.recv_array(64, dtype='uint8', check_type=False)

    @command()
    def set_speed(self, speed): pass

if __name__ == "__main__":
    import os
    import numpy as np
    import time

    host = os.getenv('HOST','192.168.1.100')
    client = connect(host, name='test_context')
    spi = TestSpi(client)
    spi.set_speed(1000000)
    spi.write(np.array([2, 4, 8, 16, 32, 64, 128, 255], dtype='uint8'))
    print spi.read(4)
    print spi.read_array()

    while True:
        print str(bin(spi.read(1))[2:]).zfill(16)
        time.sleep(0.5)
