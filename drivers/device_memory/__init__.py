# -*- coding: utf-8 -*-

import numpy as np
import re
import koheron_tcp_client as kc

class DeviceMemory(object):

    def __init__(self, client):
        self.client = client
        self.maps = {} 

    def add_mmap(self, mmap):
        @kc.command('DEVICE_MEMORY','II')
        def add_memory_map(self, device_addr, map_size):
            return self.client.recv_int32()
        if not mmap.name in self.maps:
            device_addr = int(mmap.offset, 0)
            map_size = 1024 * int(mmap.range.replace("K", ""))
            mmap_idx = add_memory_map(self, device_addr, map_size)
            if mmap_idx >= 0:
                self.maps[mmap.name] = mmap_idx

    def read32(self, device_name, offset):
        @kc.command('DEVICE_MEMORY','II')
        def read(self, mmap_idx, offset):
            return self.client.recv_uint32()
        return read(self, self.maps[device_name], offset)

    def write32(self, device_name, offset, value):
        @kc.command('DEVICE_MEMORY','III')
        def write(self, mmap_idx, offset, value):
            pass
        write(self, self.maps[device_name], offset, value)

    def write_buffer(self, device_name, offset, data, format_char='I', dtype=np.uint32):
        @kc.write_buffer('DEVICE_MEMORY', 'II', format_char=format_char, dtype=dtype)
        def write_buffer(self, data, mmap_idx, offset): 
            pass
        write_buffer(self, data, self.maps[device_name], offset)

    def read_buffer(self, device_name, offset, buff_size):
        @kc.command('DEVICE_MEMORY', 'III')
        def read_buffer(self, mmap_idx, offset, buff_size):
            return self.client.recv_buffer(buff_size)
        return read_buffer(self, self.maps[device_name], offset, buff_size)

    def set_bit(self, device_name, offset, index):
        @kc.command('DEVICE_MEMORY', 'III')
        def set_bit(self, mmap_idx, offset, index):
            pass
        set_bit(self, self.maps[device_name], offset, index)

    def clear_bit(self, device_name, offset, index):
        @kc.command('DEVICE_MEMORY', 'III')
        def clear_bit(self, mmap_idx, offset, index):
            pass
        clear_bit(self, self.maps[device_name], offset, index)

    def toggle_bit(self, device_name, offset, index):
        @kc.command('DEVICE_MEMORY', 'III')
        def toggle_bit(self, mmap_idx, offset, index):
            pass
        toggle_bit(self, self.maps[device_name], offset, index)

    def get_map_params(self, device_name):
        @kc.command('DEVICE_MEMORY', 'I')
        def get_map_params(self, mmap_idx):
            return self.client.recv_tuple('IiIIi')
        params = get_map_params(self, self.maps[device_name])
        return {
            'virt_addr': params[0],
            'status': params[1],
            'phys_addr': params[2],
            'size': params[3],
            'protection': params[4]
        }

    @kc.command('DEVICE_MEMORY')
    def get_instrument_config(self):
        return self.client.recv_json()


