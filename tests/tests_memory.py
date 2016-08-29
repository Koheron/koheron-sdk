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

test_set_clear_bit()