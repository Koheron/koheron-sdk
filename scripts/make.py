#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import jinja2
import yaml
import sys
import shutil

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
    lists = ['cores','xdc']
    for list_ in lists:
        config[list_] = get_list(project, list_)
    props = ['board','host']
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

def fill_template(config, template_filename, output_filename):
	template = get_renderer().get_template(os.path.join('scripts/templates',template_filename))
	with open(output_filename, 'w') as output:
		output.write(template.render(dic=config))

def fill_config_tcl(config):
    output_filename = os.path.join('projects', config['project'], 'config.tcl')
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

    if cmd == '--config_tcl':
        fill_config_tcl(config)

    elif cmd == '--cores':
        with open(os.path.join('tmp', project + '.cores'), 'w') as f:
            f.write(' '.join(config['cores']))

    elif cmd == '--board':
        with open(os.path.join('tmp', project + '.board'), 'w') as f:
            f.write(config['board'])

    elif cmd == '--drivers':
        drivers_filename = os.path.join('projects', project, 'drivers.yml')    
        with open(drivers_filename) as drivers_file:
            drivers = yaml.load(drivers_file) 
        with open(os.path.join('tmp', project + '.drivers'), 'w') as f:
            f.write((' '.join(drivers['drivers'])) if ('drivers' in drivers) else '')

    elif cmd == '--xdc':
    	with open(os.path.join('tmp', project + '.xdc'), 'w') as f:
            f.write(' '.join(config['xdc']))

    elif cmd == '--middleware':
        dest =  'tmp/' + project + '.middleware/drivers'
        if not os.path.exists(dest):
            os.makedirs(dest)
        fill_addresses(config, dest)

    else:
        raise ValueError('Unknown command')
