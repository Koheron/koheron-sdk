import os
import socket
import struct
import numpy as np

from koheron import KoheronClient, load_instrument
from common import Common

host = os.getenv('HOST','192.168.1.100')
project = os.getenv('NAME','')
client = load_instrument(host, project)
common = Common(client)

def ip2long(ip):
    '''Convert an IP string to long'''
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

    def test_get_server_version(self):
        assert len(common.get_server_version()) == 7

    def test_get_instrument_config(self):
        config = common.get_instrument_config()
        assert 'memory' in config
        assert 'board' in config
        assert 'config_registers' in config
        assert 'status_registers' in config
        assert 'project' in config
        assert 'xdc' in config
        assert 'parameters' in config
        assert 'cores' in config

    def test_cfg_write_read(self):
        config = common.get_instrument_config()
        value = np.random.randint(16384, size=1)[0]
        common.cfg_write(common.mem_cfg.cfg['led'], value)
        assert common.cfg_read(common.mem_cfg.cfg['led']) == value

    def test_cfg_read_all(self):
        value = np.random.randint(16384, size=1)[0]
        common.cfg_write(0, value)
        assert common.cfg_read(0) == value
        assert common.cfg_read_all()[0] == value

    def test_sts_read_all(self):
        assert common.sts_read(0) == common.sts_read_all()[0]

