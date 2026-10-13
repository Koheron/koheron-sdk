"""Decode actual BRAM RPC frames, including failed captures and legacy servers."""
import importlib.util
from pathlib import Path
import struct
import unittest

import numpy as np
from koheron import KoheronClient

PATH = Path(__file__).resolve().parents[2] / 'examples/alpha250/adc-dac-bram/adc_dac_bram.py'
spec = importlib.util.spec_from_file_location('alpha250_bram', PATH)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class Socket:
    def __init__(self, valid=True):
        self.pending = b''
        self.valid = valid
        self.calls = []
        self.words = np.array([0xffff0001, 0x00018000, 0x80007fff, 0x7fffffff], dtype='<u4')

    def sendall(self, packet):
        assert not self.pending, 'Previous reply was not completely consumed'
        reserved, driver, command = struct.unpack('>IHH', packet)
        assert reserved == 0 and driver == 2
        self.calls.append(command)
        payload = struct.pack('>I', 4) if command in (0, 1) else self.words.tobytes()
        if command == 3:
            payload = struct.pack('?', self.valid) + payload
        self.pending = packet + payload

    def recv_into(self, buffer):
        # Exercise partial and non-word-aligned reads through KoheronClient.
        size = min(len(buffer), len(self.pending), 7)
        buffer[:size] = self.pending[:size]
        self.pending = self.pending[size:]
        return size

    def close(self):
        pass


def client(checked=True, valid=True):
    c = KoheronClient.__new__(KoheronClient)
    functions = [dict(id=0,name='get_dac_size',args=[],ret_type='uint32_t'),
                 dict(id=1,name='get_adc_size',args=[],ret_type='uint32_t'),
                 dict(id=2,name='get_adc',args=[],ret_type='std::array<uint32_t, 4>')]
    if checked:
        functions.append(dict(id=3,name='get_adc_checked',args=[],ret_type='std::tuple<bool, std::array<uint32_t, 4>>'))
    c.commands = [dict(id=2,**{'class':'AdcDacBram'},functions=functions)]
    c.devices_idx = {'AdcDacBram':2}
    c.cmds_idx_list = [None,None,{f['name']:f['id'] for f in functions}]
    c.cmds_args_list = [None,None,{f['name']:f['args'] for f in functions}]
    c.cmds_ret_types_list = [None,None,{f['name']:f['ret_type'] for f in functions}]
    c.sock = Socket(valid)
    return c


class BramCaptureClientTest(unittest.TestCase):
    def test_checked_reply_and_signed_channels(self):
        c=client();driver=module.AdcDacBram(c);driver.get_adc()
        np.testing.assert_array_equal(driver.adc,[[1,-32768,32767,-1],[-1,1,-32768,32767]])
        self.assertEqual(c.sock.calls,[0,1,3])
        self.assertEqual(c.sock.pending,b'')

    def test_failure_consumes_reply_and_preserves_previous_capture(self):
        c=client();driver=module.AdcDacBram(c);driver.get_adc();previous=driver.adc.copy()
        c.sock.valid=False
        with self.assertRaisesRegex(RuntimeError,'BRAM acquisition timed out'):
            driver.get_adc()
        self.assertEqual(c.sock.pending,b'')
        np.testing.assert_array_equal(driver.adc,previous)
        c.sock.valid=True;driver.get_adc()
        self.assertEqual(c.sock.pending,b'')

    def test_legacy_server_uses_original_array_rpc(self):
        c=client(checked=False);driver=module.AdcDacBram(c);driver.get_adc()
        self.assertEqual(c.sock.calls,[0,1,2])
        np.testing.assert_array_equal(driver.adc[0],[1,-32768,32767,-1])
        self.assertEqual(c.sock.pending,b'')


if __name__=='__main__':unittest.main()
