#! /usr/bin/python

import yaml
import os

class MemMap(object):

    def __init__(self, name_, offset_, range_):
        self.name = name_
        self.offset = offset_
        self.range = range_
        self.display()

    def display(self):
        msg = 'map {} at offset {} and range {}'
        print(msg.format(self.name, self.offset, self.range))

class Config(object):
 
    def __init__(self, project):
        self.load(project)

    def load(self, project):
        self.project = project
        config_filename = os.path.join('projects', project, 'main.yml')         
        with open(config_filename) as config_file:
            dic = yaml.load(config_file)

        self.mmaps = []
        self.cfg = {}
        self.sts = {}
        
        for addr in dic['addresses']:
            self.mmaps.append(MemMap(addr['name'], addr['offset'], addr['range']))

        for i, offset in enumerate(dic['config_offsets']):
            self.cfg[offset] = 4 * i

        for i, offset in enumerate(dic['status_offsets']):
            self.sts[offset] = 4 * (10 + i)

        self.sts['bitstream_id'] = 0
        self.sts['dna'] = 4 * 8

        
