import context
import os
import socket
import struct
import numpy as np

from instrument_manager import InstrumentManager
from koheron_tcp_client import KClient
from project_config import ProjectConfig

from drivers.common import Common

host = os.getenv('HOST','192.168.1.2')
project = os.getenv('NAME','')

im = InstrumentManager(host)
im.install_instrument(project)
pc = ProjectConfig(project)

client = KClient(host)
common = Common(client)

def ip2long(ip):
    """Convert an IP string to long"""
    packedIP = socket.inet_aton(ip)
    return struct.unpack("!L", packedIP)[0]

class TestsCommon:
    def test_get_bitstream_id(self):
        assert len(common.get_bitstream_id()) == 64

    def test_set_get_led(self):
        common.set_led(42)
        assert common.get_led() == 42

    def test_ip_on_leds(self):
        common.ip_on_leds()
        assert common.get_led() == ip2long(host)

    def test_get_dna(self):
        assert len(common.get_dna()) == 14

# tests = TestsCommon()
# tests.test_ip_on_leds()