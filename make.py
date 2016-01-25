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
    config = _load_config_file(project)
    if 'parent' in config and config['parent'] != None:
        prop_list.extend(get_list(config['parent'], prop, prop_list))
    if prop in config:
        prop_list.extend(config[prop])
    # Ensure each item is only included once:
    prop_list = list(set(prop_list))
    return prop_list

def get_prop(project, prop):
    config = _load_config_file(project)
    if not prop in config:
        config[prop] = get_prop(config['parent'], prop)
    return config[prop]

def get_parents(project, parents=[]):
    config = _load_config_file(project)
    if 'parent' in config and config['parent'] != None:
        parents.append(config['parent'])
        get_parents(config['parent'], parents)
    return parents

def fill_config_tcl(config):
    template = renderer.get_template(os.path.join('projects', 'config.j2'))
    output = file(os.path.join('projects', config['project'], 'config.tcl'),'w')
    output.write(template.render(dic=config))
    output.close()

def build_middleware(project, tcp_server_dir):
    _check_project(project)
    config = _load_config_file(project)
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
      '../devices/dev_mem.yaml', 
      '../middleware/misc/init_tasks.hpp'
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
    
def fill_addresses(config, tcp_server_dir):
    template = renderer.get_template(os.path.join('projects', 'addresses.j2'))
    output = file(os.path.join(tcp_server_dir, 'middleware', 'drivers', 'addresses.hpp'),'w')
    output.write(template.render(config=config))
    output.close()

def _check_device(project, device):
    device_path = os.path.dirname(device)
    if device_path in get_parents(project) or device_path == '':
        return
    raise RuntimeError('Project ' + path + ' error for device '+ device)

def _check_project(project):
    if project in os.listdir('projects'):
        return
	raise RuntimeError('Unknown project ' + project_name)

def _load_config_file(project):
    config_filename = os.path.join('projects', project, 'main.yml')    
    with open(config_filename) as config_file:
        config = yaml.load(config_file)        
    return config

def get_config(project):
    config = _load_config_file(project)
    # Get missing elements from ancestors
    lists = ['python','cores']
    for list_ in lists:
        config[list_] = get_list(project, list_)
    props = ['board','host','xdc']
    for prop in props:
        config[prop] = get_prop(project, prop)
    return config

if __name__ == "__main__":
    project = sys.argv[1]

    tmp_dir = 'tmp'
    if not os.path.exists(tmp_dir):
        os.makedirs(tmp_dir)

    reload(sys)
    sys.setdefaultencoding('utf-8')

    renderer = jinja2.Environment(
      block_start_string = '{%',
      block_end_string = '%}',
      variable_start_string = '{{',
      variable_end_string = '}}',
      loader = jinja2.FileSystemLoader(os.path.abspath('.'))
    )

    config = get_config(project)
    fill_config_tcl(config)
    tcp_server_dir = os.path.join('tmp', config['project']+'.tcp-server')
    
    if (len(sys.argv) == 3 and sys.argv[2] == '--python'):
        with open(os.path.join('tmp', project + '.python'), 'w') as f:
            f.write(' '.join(config['python']))

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
