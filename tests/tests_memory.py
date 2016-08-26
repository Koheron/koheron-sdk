import context
import os

from koheron import KoheronClient, load_instrument
from drivers.test_memory import TestMemory

host = os.getenv('HOST','192.168.1.100')
project = os.getenv('NAME','')
client = load_instrument(host, project)
test_memory = TestMemory(client)

def test_write_read_u32():
    assert(test_memory.write_read_u32())

def test_write_read_reg_u32():
    assert(test_memory.write_read_reg_u32(offset=0))

def test_write_read_i16():
    assert(test_memory.write_read_i16())

def test_write_read_reg_i16():
    assert(test_memory.write_read_reg_i16(offset=0))

def test_write_read_float():
    assert(test_memory.write_read_float())

def test_write_read_reg_float():
    assert(test_memory.write_read_reg_float(offset=0))

def test_write_read_u32_array():
    assert(test_memory.write_read_u32_array())

def test_write_read_reg_u32_array():
    assert(test_memory.write_read_reg_u32_array(offset=0))

def test_write_read_float_array():
    assert(test_memory.write_read_float_array())

def test_write_read_reg_float_array():
    assert(test_memory.write_read_reg_float_array(offset=0))

def test_set_get_ptr_u32():
    assert(test_memory.set_get_ptr_u32())

def test_set_get_reg_ptr_u32():
    assert(test_memory.set_get_reg_ptr_u32(offset=0))

def test_set_get_ptr_float():
    assert(test_memory.set_get_ptr_float())

def test_set_get_reg_ptr_float():
    assert(test_memory.set_get_reg_ptr_float(offset=0))

def test_set_clear_bit():
    assert(test_memory.set_clear_bit())