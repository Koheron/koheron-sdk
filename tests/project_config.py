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

class ProjectConfig(object):
 
    def __init__(self, project):
        self.load(project)

    def load(self, project):
        self.project = project
        config_filename = os.path.join('projects', project, 'main.yml')         
        with open(config_filename) as config_file:
            dic = yaml.load(config_file)

        # List of memory maps
        self.mmaps = []

        # Config and Status registers
        self.cfg = {}
        self.sts = {}
        
        for addr in dic['addresses']:
            self.mmaps.append(MemMap(addr['name'], addr['offset'], addr['range']))

        for i, name in enumerate(dic['config_registers']):
            self.cfg[name] = 4 * i

        for i, name in enumerate(dic['status_registers']):
            self.sts[name] = 4 * (10 + i)

        self.sts['bitstream_id'] = 0
        self.sts['dna'] = 4 * 8

