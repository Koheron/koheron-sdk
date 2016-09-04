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

def get_list(project, prop, project_path='projects', prop_list=None):
    """ Ex: Get the list of cores needed by the 'oscillo' instrument.
    list = get_list('oscillo', 'cores')
    """
    if prop_list is None: 
       prop_list = []
    config = load_config(os.path.join(project_path, project))
    if 'parent' in config and config['parent'] != None:
        prop_list.extend(get_list(config['parent'], prop, project_path, prop_list))
    if prop in config:
        prop_list.extend(config[prop])
    config_default = load_config('projects/default')
    # Ensure each item is only included once:
    prop_list = list(set(prop_list))
    return prop_list

def get_prop(project, prop, project_path='projects'):
    config = load_config(os.path.join(project_path, project))
    if not prop in config:
        if 'parent' in config:
            config[prop] = get_prop(config['parent'], prop, project_path)
        else:
            print('Property %s not found', prop)
            return None
    return config[prop]

# def get_parents(project, parents=[]):
#     """ Get the list of parents (recursively). """
#     config = load_config(project)
#     if 'parent' in config and config['parent'] != None:
#         parents.append(config['parent'])
#         get_parents(config['parent'], parents)
#     return parents

def load_config(project_dir):
    """ Get the config dictionary from the file 'main.yml'. """
    config_filename = os.path.join(project_dir, 'main.yml')
    with open(config_filename) as config_file:
        config = yaml.load(config_file)    
    return config

def parse_brackets(string):
    """ ex: 'pwm', '4' = parse_brackets('pwm[4]') """
    start, end = map(lambda char : string.find(char), ('[',']'))
    if start >= 0 and end >= 0:
        return string[0:start], string[start+1:end]
    else:
        return string, '1'

def get_config(project, project_path='projects'):
    """ Get the config dictionary recursively. 
    ex: config = get_config('oscillo')
    """
    cfg = load_config(os.path.join(project_path, project))

    # Get missing elements from ancestors
    lists = ['cores','xdc','modules']
    for list_ in lists:
        cfg[list_] = get_list(project, list_, project_path=project_path)
    props = ['board']
    for prop in props:
        cfg[prop] = get_prop(project, prop, project_path=project_path)

    params = cfg['parameters']

    # memory
    if 'memory' in cfg:
        for addr in cfg['memory']:
            name, num = parse_brackets(addr['name'])
            if num.isdigit():
                num = int(num)
            else:
                assert(num in params)
                num = params[num]
            addr['name'] = name
            addr['n_blocks'] = num

            # Protection
            if not 'protection' in addr:
                addr['prot_flag'] = 'PROT_READ|PROT_WRITE'
            elif addr['protection'] == 'read':
                addr['prot_flag'] = 'PROT_READ'
            elif addr['protection'] == 'write':
                addr['prot_flag'] = 'PROT_WRITE'

    # Config and status registers
    lists = ['config_registers','status_registers']
    for list_ in lists:
        new_list = []
        if cfg[list_] is not None:
            for string in cfg[list_]:
                s, num = parse_brackets(string)
                if num.isdigit():
                    num = int(num)
                else:
                    assert(num in params)
                    num = params[num]
                if num == 1:
                    new_list.append(s)
                else:
                    for i in range(num):
                        new_list.append(s+str(i))
        cfg[list_] = new_list

    # Modules
    for module in cfg['modules']:
        module_cfg = get_config(module, 'fpga/modules')
        cfg['cores'].extend(module_cfg['cores'])
        cfg['cores'] = list(set(cfg['cores']))
    
    # SHA
    sha_filename = os.path.join('tmp', project + '.sha')
    if os.path.isfile(sha_filename):
        with open(sha_filename) as sha_file:
            sha = sha_file.read()
            for i in range(8):
                cfg['parameters']['sha' + str(i)] = int('0x' + sha[8*i:8*i+8], 0)

    cfg['json'] = json.dumps(cfg, separators=(',', ':')).replace('"', '\\"')
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

def fill_memory(config, drivers_dir):
    output_filename = os.path.join(drivers_dir, 'memory.hpp')
    fill_template(config, 'memory.hpp', output_filename)

def fill_start_sh(config):
    output_filename = os.path.join('tmp', config['project']+'.start.sh')
    fill_template(config, 'start.sh', output_filename)

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
    def replace_KMG(string):
        return string.replace('K','*1024').replace('M','*1024*1024').replace('G','*1024*1024*1024')
    renderer.filters['quote'] = quote
    renderer.filters['remove_extension'] = remove_extension
    renderer.filters['replace_KMG'] = replace_KMG

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
        projects = ['blink','oscillo', 'spectrum']
        for project in projects:
            print_config(project)
            test_module_consistency(project)
            test_core_consistency(project)

    elif cmd == '--config_tcl':
        config = get_config(sys.argv[2], sys.argv[3])
        assert('sha0' in config['parameters'])
        fill_config_tcl(config)

    elif cmd == '--start_sh':
        config = get_config(sys.argv[2], sys.argv[3])
        fill_start_sh(config)

    elif cmd == '--cores':
        project = sys.argv[2]
        config = get_config(project, project_path=sys.argv[3])
        cores_filename = os.path.join('tmp', project + '.cores')
        with open(cores_filename, 'w') as f:
            f.write(' '.join(config['cores']))

    elif cmd == '--board':
        project = sys.argv[2]
        config = get_config(project, project_path=sys.argv[3])
        board_filename = os.path.join('tmp', project + '.board')
        with open(board_filename, 'w') as f:
            f.write(config['board'])

    elif cmd == '--drivers':
        project = sys.argv[2]
        project_path = sys.argv[3]
        drivers_filename = os.path.join(project_path, project, 'drivers.yml')

        if os.path.isfile(drivers_filename):
            with open(drivers_filename) as drivers_file:
                drivers = yaml.load(drivers_file)
        else:
            drivers = {}

        for include_filename in drivers.get('includes', []):
            with open(include_filename) as include_file:
                for key, value in yaml.load(include_file).iteritems():
                    if key in drivers:
                        drivers[key].extend(value)
                    else:
                        drivers[key] = value

        with open(os.path.join('tmp', project + '.drivers'), 'w') as f:
            f.write((' '.join(drivers['drivers'])) if ('drivers' in drivers) else '')

    elif cmd == '--xdc':
        project = sys.argv[2]
        config = get_config(project, project_path=sys.argv[3])
        xdc_filename = os.path.join('tmp', project + '.xdc')
        with open(xdc_filename, 'w') as f:
            f.write(' '.join(config['xdc']))

    elif cmd == '--middleware':
        project = sys.argv[2]
        config = get_config(project, project_path=sys.argv[3])
        dest =  'tmp/' + project + '.middleware/drivers'
        if not os.path.exists(dest):
            os.makedirs(dest)
        fill_memory(config, dest)

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
