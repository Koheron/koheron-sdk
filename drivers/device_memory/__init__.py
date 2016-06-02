# -*- coding: utf-8 -*-

import numpy as np
import re
from koheron_tcp_client import command

class DeviceMemory(object):

    def __init__(self, client):
        self.client = client
        self.maps = {} 

    def add_map(self, device_name, offset, range):
        @command('DEVICE_MEMORY','II')
        def add_memory_map(self, device_addr, map_size):
            return self.client.recv_int32()
        if not device_name in self.maps:
            device_addr = int(offset, 0)
            map_size = 1024 * int(range.replace("K", ""))
            print device_addr, map_size
            mmap_idx = add_memory_map(self, device_addr, map_size)
            if mmap_idx >= 0:
                self.maps[device_name] = mmap_idx

    def read32(self, device_name, offset):
        @command('DEVICE_MEMORY','II')
        def read(self, mmap_idx, offset):
            return self.client.recv_int32()
        return read(self, self.maps[device_name], offset)

    def write32(self, device_name, offset, value):
        @command('DEVICE_MEMORY','III')
        def write(self, mmap_idx, offset, value):
            pass
        write(self, self.maps[device_name], offset, value)

    def write_buffer(self, device_name, offset, data, format_char='I', dtype=np.uint32):
        @write_buffer('DEVICE_MEMORY', format_char=format_char, dtype=dtype)
        def write_buffer(self, data, mmap_idx, offset): 
            pass
        write_buffer(self, data, self.maps[device_name], offset)

    def set_bit(self, device_name, offset, index):
        @command('DEVICE_MEMORY', 'III')
        def set_bit(self, mmap_idx, offset, index):
            pass
        set_bit(self, self.maps[device_name], offset, index)

    def clear_bit(self, device_name, offset, index):
        @command('DEVICE_MEMORY', 'III')
        def clear_bit(self, mmap_idx, offset, index):
            pass
        clear_bit(self, self.maps[device_name], offset, index)

    def toggle_bit(self, device_name, offset, index):
        @command('DEVICE_MEMORY', 'III')
        def toggle_bit(self, mmap_idx, offset, index):
            pass
        toggle_bit(self, self.maps[device_name], offset, index)
