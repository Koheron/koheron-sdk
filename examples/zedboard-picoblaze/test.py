#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import connect, command
import os
import subprocess

class Picoblaze(object):
    def __init__(self, client):
        self.client = client

    @command()
    def write_ram(self, address, value):
        pass

    @command()
    def reset(self):
        pass

    @command()
    def set_input(self, value):
        pass

    @command()
    def get_output(self):
        return self.client.recv_uint32()

host = os.getenv('HOST','192.168.1.23')
client = connect(host, name='picoblaze')
driver = Picoblaze(client)

code = """
input s0, 0
add s0, {:x}
output s0, 0
"""

for i in range(100):

    with open("test.psm", "w") as f:
        f.write(code.format(i))

    # Compile test.psm with opbasm
    subprocess.call(['opbasm', '--pb6', '--scratch-size=64', '--hex', '--mem-size=32', '--quiet', 'test.psm'])

    # Write program to BRAM
    with open('test.hex', 'r') as f:
        for j, line in enumerate(f):
            driver.write_ram(4*j, (int(line, 16)))

    driver.set_input(2)
    #driver.reset() # Restart picoblaze
    print i, driver.get_output()
    assert(driver.get_output() == 2 + i)
