# -*- coding: utf-8 -*-

import numpy as np
import re
import koheron_tcp_client as kc

class MemMap(object):
    def __init__(self, id_, name_, offset_, range_):
        self.id = id_
        self.name = name_
        self.offset = offset_
        self.range = range_
        # self.display()

    def display(self):
        msg = 'map {} at offset {} and range {}'
        print(msg.format(self.name, self.offset, self.range))

class MemoryConfig(object):
     def __init__(self, dic):
        # List of memory maps
        self.mmaps = {}
        # Config and Status registers
        self.cfg = {}
        self.sts = {}

        for idx, addr in enumerate(dic['addresses']):
            self.mmaps[addr['name']] = MemMap(idx, addr['name'], addr['offset'], addr['range'])

        for i, name in enumerate(dic['config_registers']):
            self.cfg[name] = 4 * i

        for i, name in enumerate(dic['status_registers']):
            self.sts[name] = 4 * (10 + i)

        self.sts['bitstream_id'] = 0
        self.sts['dna'] = 4 * 8

class DeviceMemory(object):
    def __init__(self, client):
        self.client = client
        self.memory_cfg = MemoryConfig(self.get_instrument_config())

    def get_id(self, map_name):
        return self.memory_cfg.mmaps[map_name].id

    def read32(self, map_name, offset):
        @kc.command('DEVICE_MEMORY','II')
        def read(self, mmap_idx, offset):
            return self.client.recv_uint32()
        return read(self, self.get_id(map_name), offset)

    def write32(self, map_name, offset, value):
        @kc.command('DEVICE_MEMORY','III')
        def write(self, mmap_idx, offset, value):
            pass
        write(self, self.get_id(map_name), offset, value)

    def write_buffer(self, map_name, offset, data, format_char='I', dtype=np.uint32):
        @kc.write_buffer('DEVICE_MEMORY', 'II', format_char=format_char, dtype=dtype)
        def write_buffer(self, data, mmap_idx, offset): 
            pass
        write_buffer(self, data, self.get_id(map_name), offset)

    def read_buffer(self, map_name, offset, buff_size):
        @kc.command('DEVICE_MEMORY', 'III')
        def read_buffer(self, mmap_idx, offset, buff_size):
            return self.client.recv_buffer(buff_size)
        return read_buffer(self, self.get_id(map_name), offset, buff_size)

    def set_bit(self, map_name, offset, index):
        @kc.command('DEVICE_MEMORY', 'III')
        def set_bit(self, mmap_idx, offset, index):
            pass
        set_bit(self, self.get_id(map_name), offset, index)

    def clear_bit(self, map_name, offset, index):
        @kc.command('DEVICE_MEMORY', 'III')
        def clear_bit(self, mmap_idx, offset, index):
            pass
        clear_bit(self, self.get_id(map_name), offset, index)

    def toggle_bit(self, map_name, offset, index):
        @kc.command('DEVICE_MEMORY', 'III')
        def toggle_bit(self, mmap_idx, offset, index):
            pass
        toggle_bit(self, self.get_id(map_name), offset, index)

    def get_map_params(self, map_name):
        @kc.command('DEVICE_MEMORY', 'I')
        def get_map_params(self, mmap_idx):
            return self.client.recv_tuple('IiIIi')
        params = get_map_params(self, self.get_id(map_name))
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


