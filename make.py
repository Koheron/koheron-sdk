#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import sys
import json
import jinja2
import yaml
import server

def append_path(filename, file_path):
    ''' If a filename starts with './' then it is relative to the config.yml path.
    '''
    if filename.startswith('./'):
        filename = os.path.join(file_path, filename)
    else:
        filename = os.path.join(SDK_PATH, filename)

    return filename

def load_config(config_filename):
    ''' Get the config dictionary from the file 'config.yml' '''

    with open(config_filename) as f:
        config = yaml.load(f)

    return config

def parse_brackets(string):
    ''' ex: 'pwm', '4' = parse_brackets('pwm[4]') '''
    start, end = map(lambda char : string.find(char), ('[', ']'))
    if start >= 0 and end >= 0:
        return string[0 : start], string[start + 1 : end]
    else:
        return string, '1'

def read_parameters(string, parameters):

    string, parameter = parse_brackets(string)

    if parameter.isdigit():
        parameter = int(parameter)
    else:
        assert parameter in parameters
        parameter = parameters[parameter]

    return string, parameter

def build_memory(memory, parameters):

    for address in memory:

        address['name'], address['n_blocks'] = read_parameters(address['name'], parameters)
        assert (address['n_blocks'] > 0)

        # Protection
        if not 'protection' in address:
            address['prot_flag'] = 'PROT_READ|PROT_WRITE'
        elif address['protection'] == 'read':
            address['prot_flag'] = 'PROT_READ'
        elif address['protection'] == 'write':
            address['prot_flag'] = 'PROT_WRITE'

    return memory

def build_registers(registers, parameters):

    new_registers = []

    for register in registers:
        register, parameter = read_parameters(register, parameters)

        if parameter == 1:
            new_registers.append(register)
        else:
            for i in range(parameter):
                new_registers.append(register+str(i))

    registers = new_registers

    return registers

def build_json(dict):
    dict_json = json.dumps(dict, separators=(',', ':')).replace('"', '\\"')
    return dict_json

def dump_if_changed(filename, new_dict):
    changed = False
    if os.path.isfile(filename):
        with open(filename, 'r') as yml_file:
            old_dict = yaml.load(yml_file)
            if old_dict != new_dict:
               changed = True

    if not os.path.isfile(filename) or changed:
        with open(filename, 'w') as yml_file:
            yaml.dump(new_dict, yml_file)

#########################
# Jinja2 template engine
#########################

def get_renderer():
    renderer = jinja2.Environment(
      block_start_string = '{%',
      block_end_string = '%}',
      variable_start_string = '{{',
      variable_end_string = '}}',
      loader = jinja2.FileSystemLoader([os.path.join(SDK_PATH, 'fpga'), os.path.join(SDK_PATH, 'server/templates')])
    )

    def quote(list_):
        return ['"%s"' % element for element in list_]

    def remove_extension(filename):
        toks = filename.split('.')
        return toks[0]

    def replace_KMG(string):
        return string.replace('K', '*1024').replace('M', '*1024*1024').replace('G', '*1024*1024*1024')

    renderer.filters['quote'] = quote
    renderer.filters['remove_extension'] = remove_extension
    renderer.filters['replace_KMG'] = replace_KMG
    return renderer

def fill_template(config, template_filename, output_filename):
    template = get_renderer().get_template(template_filename)
    with open(output_filename, 'w') as output:
        output.write(template.render(config=config))

###################
# Main
###################

SDK_PATH = os.getenv('SDK_PATH', '')

if __name__ == "__main__":

    cmd = sys.argv[1]
    config_filename = sys.argv[2]
    output_filename = sys.argv[3]

    output_dirname = os.path.dirname(output_filename)
    if not os.path.exists(output_dirname):
        os.makedirs(output_dirname)

    config = load_config(config_filename)
    config_path = os.path.dirname(config_filename)

    reload(sys)
    sys.setdefaultencoding('utf-8')

    if cmd == '--name':
        with open(output_filename, 'w') as f:
            f.write(config['name'])

    elif cmd == '--memory_yml':
        for field in ['drivers', 'web', 'cores', 'modules', 'name', 'board', 'version']:
            config.pop(field, None)
        dump_if_changed(output_filename, config)

    elif cmd == '--config_tcl':
        parameters = config.get('parameters', {})
        config['memory'] = build_memory(config.get('memory', {}), parameters)
        config['control_registers'] = build_registers(config.get('control_registers', {}), parameters)
        config['status_registers'] = build_registers(config.get('status_registers', {}), parameters)
        fill_template(config, 'config.tcl', output_filename)

    elif cmd == '--start_sh':
        fill_template(config, 'start.sh', output_filename)

    elif cmd == '--cores':

        for module in config.get('modules', []):
            module_path = os.path.dirname(module)
            module = append_path(module, module_path)
            module_config = load_config(module)
            module_cores = module_config.get('cores')
            if module_cores is not None:
                config['cores'].extend(module_cores)
            config['cores'] = list(set(config['cores']))

        for i in range(len(config['cores'])):
            config['cores'][i] = append_path(config['cores'][i], config_path)

        with open(output_filename, 'w') as f:
            f.write(' '.join(config.get('cores', [])))

    elif cmd == '--board':
        config['board'] = append_path(config['board'], config_path)
        with open(output_filename, 'w') as f:
            f.write(config['board'])

    elif cmd == '--drivers':
        for i, path in enumerate(config.get('drivers', [])):
            config['drivers'][i] = append_path(path, config_path)
        with open(output_filename, 'w') as f:
            f.write(' '.join(config.get('drivers', [])))

    elif cmd == '--xdc':
        for i, path in enumerate(config.get('xdc', [])):
            config['xdc'][i] = append_path(path, config_path)
        with open(output_filename, 'w') as f:
            f.write(' '.join(config.get('xdc', [])))

    elif cmd == '--memory_hpp':
        parameters = config.get('parameters', {})
        config['memory'] = build_memory(config.get('memory', {}), parameters)
        config['control_registers'] = build_registers(config.get('control_registers', {}), parameters)
        config['status_registers'] = build_registers(config.get('status_registers', {}), parameters)
        config['json'] = build_json(config)
        fill_template(config, 'memory.hpp', output_filename)

    elif cmd == '--render_template':
        template_filename = sys.argv[4]
        for i in range(len(config['drivers'])):
            config['drivers'][i] = append_path(config['drivers'][i], config_path)
        server.render_template(template_filename, output_filename, server.get_drivers(config['drivers']))

    elif cmd == '--render_interface':
        driver_filename_hpp = sys.argv[4]
        id_ = server.get_driver_id(config['drivers'], driver_filename_hpp)
        server.render_driver(server.get_driver(driver_filename_hpp, id_), output_filename)

    elif cmd == '--web':
        for i, path in enumerate(config.get('web', [])):
            config['web'][i] = append_path(path, config_path)
        with open(output_filename, 'w') as f:
            f.write(' '.join(config.get('web', [])))

    elif cmd == '--version':
        config['version'] = config.get('version', '0.0.0')
        with open(output_filename, 'w') as f:
            f.write(config['version'])

    else:
        raise ValueError('Unknown command')