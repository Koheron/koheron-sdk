#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import jinja2
import yaml
import sys
import shutil
import time
import socket
import getpass
import json

def get_list(project, prop, prop_list=None):
    """ Ex: Get the list of cores needed by the 'oscillo' instrument.
    list = get_list('oscillo', 'cores')
    """
    if prop_list is None: 
       prop_list = []
    config = load_config(project)
    if 'parent' in config and config['parent'] != None:
        prop_list.extend(get_list(config['parent'], prop, prop_list))
    if prop in config:
        prop_list.extend(config[prop])
    config_default = load_config('default')
    # Ensure each item is only included once:
    prop_list = list(set(prop_list))
    return prop_list

def get_prop(project, prop):
    """ Ex: Get the board needed by the 'oscillo' instrument.
    board = get_prop('oscillo', 'board')
    """
    config = load_config(project)
    if not prop in config:
        if 'parent' in config:
            config[prop] = get_prop(config['parent'], prop)
        else:
            print('Property %s not found', prop)
            return None
    return config[prop]

def get_parents(project, parents=[]):
    """ Get the list of parents (recursively). """
    config = load_config(project)
    if 'parent' in config and config['parent'] != None:
        parents.append(config['parent'])
        get_parents(config['parent'], parents)
    return parents

def load_config(project):
    """ Get the config dictionary from the file 'main.yml'. """
    assert project in os.listdir('projects')
    config_filename = os.path.join('projects', project, 'main.yml')
    assert(os.path.isfile(config_filename))
    with open(config_filename) as config_file:
        config = yaml.load(config_file)        
    return config

def get_config(project):
    """ Get the config dictionary recursively. 
    ex: config = get_config('oscillo')
    """
    cfg = load_config(project)
    # Get missing elements from ancestors
    lists = ['cores','xdc','modules']
    for list_ in lists:
        cfg[list_] = get_list(project, list_)
    props = ['board','host']
    for prop in props:
        cfg[prop] = get_prop(project, prop)

    # Modules
    for module in cfg['modules']:
        module_cfg = get_config(module)
        cfg['cores'].extend(module_cfg['cores'])
    
    # SHA
    sha_filename = os.path.join('tmp', project + '.sha')
    if os.path.isfile(sha_filename):
        with open(sha_filename) as sha_file:
            sha = sha_file.read()
            for i in range(8):
                cfg['parameters']['sha' + str(i)] = int('0x' + sha[8*i:8*i+8], 0)
    return cfg

###################
# Jinja
###################

def fill_template(config, template_filename, output_filename):
    template = get_renderer().get_template(os.path.join('scripts/templates', template_filename))
    with open(output_filename, 'w') as output:
        output.write(template.render(dic=config))

def fill_config_tcl(config):
    output_filename = os.path.join('tmp', config['project']+'.config.tcl')
    fill_template(config, 'config.tcl', output_filename)

def fill_addresses(config, drivers_dir):
    output_filename = os.path.join(drivers_dir, 'addresses.hpp')
    fill_template(config, 'addresses.hpp', output_filename)

def get_renderer():
    renderer = jinja2.Environment(
      block_start_string = '{%',
      block_end_string = '%}',
      variable_start_string = '{{',
      variable_end_string = '}}',
      loader = jinja2.FileSystemLoader(os.path.abspath('.'))
    )
    def quote(list_):
        return ['"%s"' % element for element in list_]
    def remove_extension(filename):
        toks = filename.split('.')
        return toks[0]
    renderer.filters['quote'] = quote
    renderer.filters['remove_extension'] = remove_extension

    return renderer

###################
# Test
###################

# Remove numbers from string
strip_num = lambda string:''.join([char for char in string if char not in "0123456789"])

def test_module_consistency(project):
    """ Check that the modules registers are defined in the project main.yml."""
    cfg = get_config(project)
    props = ['config_registers', 'status_registers']
    for module in cfg['modules']:
        module_cfg = get_config(module)
        for prop in props:
            a = module_cfg[prop]
            a = a if a is not None else []
            b = map(strip_num, set(cfg[prop]))
            assert set(a).issubset(b)

def test_core_consistency(project):
    """ Check that the modules cores are defined in the project main.yml."""
    cfg = get_config(project)
    for module in cfg['modules']:
        module_cfg = get_config(module)
        assert set(module_cfg['cores']).issubset(cfg['cores'])

def print_config(project):
    cfg = get_config(project)
    print('PROJECT = {}'.format(cfg['project']))
    print yaml.dump(cfg, default_flow_style=False)

###################
# Main
###################

if __name__ == "__main__":
    cmd = sys.argv[1]

    tmp_dir = 'tmp'
    if not os.path.exists(tmp_dir):
        os.makedirs(tmp_dir)

    reload(sys)
    sys.setdefaultencoding('utf-8')

    if cmd == '--test':
        projects = ['oscillo', 'spectrum', 'pid']
        for project in projects:
            print_config(project)
            test_module_consistency(project)
            test_core_consistency(project)

    elif cmd == '--config_tcl':
        config = get_config(sys.argv[2])
        assert('sha0' in config['parameters'])
        fill_config_tcl(config)

    elif cmd == '--cores':
        project = sys.argv[2]
        config = get_config(project)
        cores_filename = os.path.join('tmp', project + '.cores')
        with open(cores_filename, 'w') as f:
            f.write(' '.join(config['cores']))

    elif cmd == '--board':
        project = sys.argv[2]
        config = get_config(project)
        board_filename = os.path.join('tmp', project + '.board')
        with open(board_filename, 'w') as f:
            f.write(config['board'])

    elif cmd == '--drivers':
        project = sys.argv[2]
        drivers_filename = os.path.join('projects', project, 'drivers.yml')
        assert(os.path.isfile(drivers_filename))
        with open(drivers_filename) as drivers_file:
            drivers = yaml.load(drivers_file)
        with open(os.path.join('tmp', project + '.drivers'), 'w') as f:
            f.write((' '.join(drivers['drivers'])) if ('drivers' in drivers) else '')

    elif cmd == '--xdc':
        project = sys.argv[2]
        config = get_config(project)
        xdc_filename = os.path.join('tmp', project + '.xdc')
        with open(xdc_filename, 'w') as f:
            f.write(' '.join(config['xdc']))

    elif cmd == '--middleware':
        project = sys.argv[2]
        config = get_config(project)
        dest =  'tmp/' + project + '.middleware/drivers'
        if not os.path.exists(dest):
            os.makedirs(dest)
        fill_addresses(config, dest)

    # -- HTTP API

    elif cmd == '--metadata':
        metadata = {
          'version': sys.argv[3],
          'date': time.strftime("%d/%m/%Y"),
          'time': time.strftime("%H:%M:%S"),
          'machine': socket.gethostname(),
          'user': getpass.getuser()
        }

        with open('tmp/metadata.json', 'w') as f:
            json.dump(metadata, f)

    else:
        raise ValueError('Unknown command')
