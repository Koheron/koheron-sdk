# -*- coding: utf-8 -*-

from koheron import command

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
