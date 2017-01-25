import os
from koheron import command, connect

class TestMemory(object):
    def __init__(self, client):
        self.client = client

    @command()
    def write_read_u32(self):
        return self.client.recv_bool()

    @command()
    def write_read_reg_u32(self, offset):
        return self.client.recv_bool()

    @command()
    def write_read_u64(self):
        return self.client.recv_bool()

    @command()
    def write_read_reg_u64(self, offset):
        return self.client.recv_bool()

    @command()
    def write_read_i16(self):
        return self.client.recv_bool()

    @command()
    def write_read_reg_i16(self, offset):
        return self.client.recv_bool()

    @command()
    def write_read_float(self):
        return self.client.recv_bool()

    @command()
    def write_read_reg_float(self, offset):
        return self.client.recv_bool()

    @command()
    def write_read_double(self):
        return self.client.recv_bool()

    @command()
    def write_read_reg_double(self, offset):
        return self.client.recv_bool()

    @command()
    def write_read_u32_array(self):
        return self.client.recv_bool()

    @command()
    def write_read_reg_u32_array(self, offset):
        return self.client.recv_bool()

    @command()
    def write_read_float_array(self):
        return self.client.recv_bool()

    @command()
    def write_read_reg_float_array(self, offset):
        return self.client.recv_bool()

    @command()
    def set_get_ptr_u32(self):
        return self.client.recv_bool()

    @command()
    def set_get_reg_ptr_u32(self, offset):
        return self.client.recv_bool()

    @command()
    def set_get_ptr_float(self):
        return self.client.recv_bool()

    @command()
    def set_get_reg_ptr_float(self, offset):
        return self.client.recv_bool()

    @command('TestMemory')
    def set_clear_bit(self):
        return self.client.recv_bool()

    @command()
    def set_clear_reg_bit(self, offset, index):
        return self.client.recv_bool()

    @command('TestMemory')
    def toggle_bit(self):
        return self.client.recv_bool()

    @command()
    def toggle_reg_bit(self, offset, index):
        return self.client.recv_bool()

    @command()
    def read_write_bit(self):
        return self.client.recv_bool()

    @command()
    def read_write_reg_bit(self, offset, index):
        return self.client.recv_bool()

host = os.getenv('HOST', '192.168.1.100')
instrument = os.getenv('NAME', 'test_context')
client = connect(host, name=instrument)
test_memory = TestMemory(client)

def test_write_read_u32():
    assert(test_memory.write_read_u32())

def test_write_read_reg_u32():
    assert(test_memory.write_read_reg_u32(0))

def test_write_read_u64():
    assert(test_memory.write_read_u64())

def test_write_read_reg_u64():
    assert(test_memory.write_read_reg_u64(0))

def test_write_read_i16():
    assert(test_memory.write_read_i16())

def test_write_read_reg_i16():
    assert(test_memory.write_read_reg_i16(0))

def test_write_read_float():
    assert(test_memory.write_read_float())

def test_write_read_reg_float():
    assert(test_memory.write_read_reg_float(0))

def test_write_read_double():
    assert(test_memory.write_read_double())

def test_write_read_reg_double():
    assert(test_memory.write_read_reg_double(0))

def test_write_read_u32_array():
    assert(test_memory.write_read_u32_array())

def test_write_read_reg_u32_array():
    assert(test_memory.write_read_reg_u32_array(0))

def test_write_read_float_array():
    assert(test_memory.write_read_float_array())

def test_write_read_reg_float_array():
    assert(test_memory.write_read_reg_float_array(0))

def test_set_get_ptr_u32():
    assert(test_memory.set_get_ptr_u32())

def test_set_get_reg_ptr_u32():
    assert(test_memory.set_get_reg_ptr_u32(0))

def test_set_get_ptr_float():
    assert(test_memory.set_get_ptr_float())

def test_set_get_reg_ptr_float():
    assert(test_memory.set_get_reg_ptr_float(0))

def test_set_clear_bit():
    assert(test_memory.set_clear_bit())

def test_set_clear_reg_bit():
    assert(test_memory.set_clear_reg_bit(0, 15))

def test_toggle_bit():
    assert(test_memory.toggle_bit())

def test_toggle_reg_bit():
    assert(test_memory.toggle_reg_bit(0, 14))

def test_read_write_bit():
    assert(test_memory.read_write_bit())

def test_read_write_reg_bit():
    assert(test_memory.read_write_reg_bit(0, 15))