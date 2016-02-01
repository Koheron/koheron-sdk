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
        config[prop] = get_prop(config['parent'], prop)
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
    lists = ['python','cores']
    for list_ in lists:
        config[list_] = get_list(project, list_)
    props = ['board','host','xdc']
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
# Jinja:
###################

def fill_config_tcl(config):
    template = get_renderer().get_template(os.path.join('projects', 'config.j2'))
    output = file(os.path.join('projects', config['project'], 'config.tcl'),'w')
    output.write(template.render(dic=config))
    output.close()

def fill_addresses(config, tcp_server_dir):
    template = get_renderer().get_template(os.path.join('projects', 'addresses.j2'))
    output = file(os.path.join(tcp_server_dir, 'middleware', 'drivers', 'addresses.hpp'),'w')
    output.write(template.render(config=config))
    output.close()

def get_renderer():
    renderer = jinja2.Environment(
      block_start_string = '{%',
      block_end_string = '%}',
      variable_start_string = '{{',
      variable_end_string = '}}',
      loader = jinja2.FileSystemLoader(os.path.abspath('.'))
    )
    return renderer

###################
# Build directories
###################

def build_middleware(project, tcp_server_dir):
    _check_project(project)
    config = load_config(project)
    assert os.path.exists(os.path.join(tcp_server_dir,'middleware'))
    
    # We recursively traverse the requirements and build the middleware
    if 'parent' in config and config['parent'] != None:
        build_middleware(config['parent'], tcp_server_dir)
        
    for basename in os.listdir(os.path.join('projects', project)):
        if basename.endswith('.hpp') or basename.endswith('.cpp'):
            import_filename = os.path.join('projects', project, basename)
            shutil.copy(import_filename, os.path.join(tcp_server_dir,'middleware','drivers'))

def build_server_config(project, tcp_server_dir):
    config = get_config(project)
    dev_paths = [
      '../devices/dev_mem.yaml'
    ]    
    for device in config['devices']:
        _check_device(project, device)
        filename = os.path.basename(device)
        dev_paths.append(os.path.join('../middleware/drivers/', filename))
    server_config = {
      'host': config['host'],
      'devices': dev_paths
    }
    with open(os.path.join(tcp_server_dir,'config','config.yaml'), 'w') as f:
        yaml.dump(server_config, f, indent=2, default_flow_style=False)

def build_python(project, python_dir):
    if not os.path.exists(python_dir):
        os.makedirs(python_dir)

    parents = get_parents(project)
    parents.append(project)
    for parent in parents:
        config = load_config(parent)
        if config.has_key('python'):
            for py_file in config['python']:
                shutil.copy(os.path.join('projects',parent,py_file), python_dir)
                    
    _build_init_file(python_dir)
                
def _build_init_file(python_dir):
    ''' Build Python package __init__.py '''
    # TODO Put in jinja file
    
    init_filename = os.path.join(python_dir, '__init__.py')
    
    with open(init_filename, 'w') as init_file:
        for name in os.listdir(python_dir):
            if name.endswith(".py") and name != "__init__.py":
                module = name[:-3]
                init_file.write("from " + module + " import *\n")

        init_file.write("\n__all__ = [\n")
        is_first = True
        
        for name in os.listdir(python_dir):
            if name.endswith(".py") and name != "__init__.py":
                module = name[:-3]
                if is_first:
                    init_file.write('  "' + module + '"')
                    is_first = False
                else:
                    init_file.write(',\n  "' + module + '"')
                
        init_file.write("\n]\n")     

###################
# Check
###################

def _check_device(project, device):
    device_path = os.path.dirname(device)
    if device_path in get_parents(project) or device_path == '':
        return
    raise RuntimeError('Project ' + path + ' error for device '+ device)

def _check_project(project):
    if project in os.listdir('projects'):
        return
	raise RuntimeError('Unknown project ' + project_name)

###################
# Main
###################

if __name__ == "__main__":
    project = sys.argv[1]

    tmp_dir = 'tmp'
    if not os.path.exists(tmp_dir):
        os.makedirs(tmp_dir)

    reload(sys)
    sys.setdefaultencoding('utf-8')

    config = get_config(project)
    fill_config_tcl(config)
    tcp_server_dir = os.path.join('tmp', config['project']+'.tcp-server')
    python_dir = os.path.join('tmp', config['project']+'.python')
  
    if (len(sys.argv) == 3 and sys.argv[2] == '--python'):
        build_python(project, python_dir)

    if (len(sys.argv) == 3 and sys.argv[2] == '--cores'):
        with open(os.path.join('tmp', project + '.cores'), 'w') as f:
            f.write(' '.join(config['cores']))

    if (len(sys.argv) == 3 and sys.argv[2] == '--xdc'):
        shutil.copyfile(config['xdc'], os.path.join('tmp', config['project']+'.xdc'))

    if (len(sys.argv) == 3 and sys.argv[2] == '--board'):
        with open(os.path.join('tmp', project + '.board'), 'w') as f:
            f.write(config['board'])

    if (len(sys.argv) == 3 and sys.argv[2] == '--middleware'):
        build_middleware(project, tcp_server_dir)
        build_server_config(project, tcp_server_dir)
        fill_addresses(config, tcp_server_dir)
