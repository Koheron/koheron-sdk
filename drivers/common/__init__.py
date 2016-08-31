#!/usr/bin/env python
# -*- coding: utf-8 -*-

from koheron import command

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

        for idx, addr in enumerate(dic['memory']):
            self.mmaps[addr['name']] = MemMap(idx, addr['name'], addr['offset'],
                                            eval(addr['range'].replace('K','*1024')
                                                              .replace('M','*1024*1024')
                                                              .replace('G','*1024*1024*1024')))

        for i, name in enumerate(dic['config_registers']):
            self.cfg[name] = 4 * i

        for i, name in enumerate(dic['status_registers']):
            self.sts[name] = 4 * (10 + i)

        self.sts['bitstream_id'] = 0
        self.sts['dna'] = 4 * 8

class Common(object):

    def __init__(self, client):
        self.client = client
        self.mem_cfg = MemoryConfig(self.get_instrument_config())

    @command()
    def get_bitstream_id(self):
        id_array = self.client.recv_array(8, dtype='uint32')
        return ''.join('{:08x}'.format(i) for i in id_array)

    @command()
    def get_dna(self):
        id_array = self.client.recv_array(2, dtype='uint32')
        return ''.join('{:02x}'.format(i) for i in id_array)

    @command()
    def set_led(self, value): pass

    @command()
    def get_led(self):
        return self.client.recv_uint32()

    @command()
    def init(self): pass

    @command()
    def ip_on_leds(self): pass

    def get_server_version(self):
        @command(classname='KServer')
        def get_version(self):
            return self.client.recv_string()

        return get_version(self)

    @command()
    def cfg_write(self, offset, value):
        pass

    @command()
    def cfg_read(self, offset):
        return self.client.recv_uint32()

    @command()
    def sts_read(self, offset):
        return self.client.recv_uint32()

    @command()
    def cfg_read_all(self):
        return self.client.recv_array(self.mem_cfg.mmaps['config'].range/4, dtype='uint32')

    @command()
    def sts_read_all(self):
        return self.client.recv_array(self.mem_cfg.mmaps['status'].range/4, dtype='uint32')

    @command()
    def get_instrument_config(self):
        return self.client.recv_json()

    def status(self):
       print('bitstream id = {}'.format(self.get_bitstream_id()))
       print('DNA = {}'.format(self.get_dna()))
       print('LED = {}'.format(self.get_led() % 256))
