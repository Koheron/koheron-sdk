#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import jinja2
import yaml
import sys
import shutil
import time
from subprocess import call

def get_list(project, prop, prop_list=None):
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
    config = load_config(project)
    if not prop in config:
        if 'parent' in config:
            config[prop] = get_prop(config['parent'], prop)
        else:
            print('Property %s not found', prop)
            return None
    return config[prop]

def get_parents(project, parents=[]):
    config = load_config(project)
    if 'parent' in config and config['parent'] != None:
        parents.append(config['parent'])
        get_parents(config['parent'], parents)
    return parents

def load_config(project):
    config_filename = os.path.join('projects', project, 'main.yml')    
    with open(config_filename) as config_file:
        config = yaml.load(config_file)        
    return config

def get_config(project):
    config = load_config(project)
    # Get missing elements from ancestors
    lists = ['python','cores','xdc']
    for list_ in lists:
        config[list_] = get_list(project, list_)
    props = ['board','cross-compile','host']
    for prop in props:
        config[prop] = get_prop(project, prop)
    sha_filename = os.path.join('tmp', project+'.sha')
    if os.path.isfile(sha_filename):
        with open(sha_filename) as sha_file:
            sha = sha_file.read()
            for i in range(8):
                config['parameters']['sha'+str(i)] = int('0x'+sha[8*i:8*i+8],0)
    return config

###################
# Jinja
###################

def fill_config_tcl(config):
    template = get_renderer().get_template(os.path.join('templates', 'config.tcl'))
    output = file(os.path.join('projects', config['project'], 'config.tcl'),'w')
    output.write(template.render(dic=config))
    output.close()

def fill_config_python(config, version):
    template = get_renderer().get_template(os.path.join('templates', 'config.py'))
    output = file(os.path.join('projects', config['project'], 'config.py'),'w')
    output.write(template.render(dic=config, version=version))
    output.close()

def fill_addresses(config, tcp_server_dir):
    template = get_renderer().get_template(os.path.join('templates', 'addresses.hpp'))
    output = file(os.path.join(tcp_server_dir, 'middleware', 'drivers', 'addresses.hpp'),'w')
    output.write(template.render(dic=config))
    output.close()

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
# Build directories
###################
            
def build_server_config(project, tcp_server_dir):
    config = get_config(project)
    dev_paths = [
      '../devices/dev_mem.yaml',
      '../middleware/drivers/common.hpp'
    ]
    if 'drivers' in config:
        for device in config['drivers']:
            filename = os.path.basename(device) + ".hpp"
            dev_paths.append(os.path.join('../middleware/drivers/', filename))
    server_config = {
      'cross-compile': config['cross-compile'],
      'devices': dev_paths
    }
    with open(os.path.join(tcp_server_dir,'config','config.yaml'), 'w') as f:
        yaml.dump(server_config, f, indent=2, default_flow_style=False)

def build_python(project, python_dir):
    if not os.path.exists(python_dir):
        os.makedirs(python_dir)
    include_list = []
    parents = get_parents(project)
    parents.append(project)
    for parent in parents:
        config = load_config(parent)
        if 'devices' in config:
            for device in config['devices']:
                file_ = os.path.join(device, os.path.basename(device) + '.py')
                if os.path.exists(file_):
                    shutil.copy(file_, python_dir)
                    include_list.append(os.path.basename(file_).split('.')[0])
        if 'python' in config:
            for file_ in config['python']:
                if os.path.exists(file_):
                    shutil.copy(file_, python_dir)
                    include_list.append(os.path.basename(file_).split('.')[0])
                else:
                    raise ValueError("Unknown Python file: " + file_)

    template = get_renderer().get_template(os.path.join('templates', '__init__.py'))
    config = load_config(project)
    output = file(os.path.join(python_dir, '__init__.py'), 'w')
    output.write(template.render(dic={'include': include_list}))
    output.close()

def build_xdc(project, xdc_dir):
    config = get_config(project)
    if not os.path.exists(xdc_dir):
        os.makedirs(xdc_dir)
    for file_ in config['xdc']:
        shutil.copy(file_, xdc_dir)

###################
# Main
###################

if __name__ == "__main__":
    cmd = sys.argv[1]
    project = sys.argv[2]

    if project not in os.listdir('projects'):
	    raise RuntimeError('Unknown project ' + project)

    tmp_dir = 'tmp'
    if not os.path.exists(tmp_dir):
        os.makedirs(tmp_dir)

    reload(sys)
    sys.setdefaultencoding('utf-8')

    config = get_config(project)

    if cmd == '--config_py':
        version = sys.argv[3]        
        fill_config_python(config, version)
    elif cmd == '--config_tcl':
        fill_config_tcl(config)
    elif cmd == '--python':
        python_dir = os.path.join('tmp', config['project'] + '.python')
        build_python(project, python_dir)
    elif cmd == '--cores':
        with open(os.path.join('tmp', project + '.cores'), 'w') as f:
            f.write(' '.join(config['cores']))
    elif cmd == '--board':
        with open(os.path.join('tmp', project + '.board'), 'w') as f:
            f.write(config['board'])
    elif cmd == '--drivers':
        with open(os.path.join('tmp', project + '.drivers'), 'w') as f:
            f.write('common ' + ((' '.join(config['drivers'])) if ('drivers' in config) else ''))
    elif cmd == '--middleware':
        tcp_server_dir = os.path.join('tmp', config['project'] + '.tcp-server')
        build_server_config(project, tcp_server_dir)
        fill_addresses(config, tcp_server_dir)
    elif cmd == '--xdc':
        xdc_dir = os.path.join('tmp', config['project'] + '.xdc')
        build_xdc(project, xdc_dir)
    else:
        raise ValueError('Unknown command')
