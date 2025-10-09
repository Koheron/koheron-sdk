#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import sys
import json
import jinja2
import yaml
import server

def write_if_changed(path, text):
    old = None
    if os.path.isfile(path):
        with open(path, 'r') as f:
            old = f.read()
    if old != text:
        with open(path, 'w') as f:
            f.write(text)
        return True
    return False

def append_path(filename, file_path):
    if filename.startswith('./'):
        return os.path.join(file_path, filename[2:])
    if os.path.isabs(filename):          # <-- keep absolute paths
        return filename
    return os.path.join(SDK_PATH, filename)

    return filename

def load_config(config_filename):
    ''' Get the config dictionary from the file 'memory.yml' '''

    with open(config_filename) as f:
        config = yaml.safe_load(f)

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

        if not 'dev' in address:
            address['dev'] = '/dev/mem'
        elif address['dev'] == '/dev/mem_wc':
            address['dev'] = address['dev'] + address['offset']

        registers = address.get('registers', []) or []
        registers = build_registers(registers, parameters) if registers else []
        address['registers'] = registers
        address['register_count'] = len(registers)

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

def append_memory_to_config(config):
    parameters = config.get('parameters', {})
    config['memory'] = build_memory(config.get('memory', {}), parameters)
    return config

def build_json(dict):
    dict_json = json.dumps(dict, separators=(',', ':')).replace('"', '\\"')
    return dict_json

def dump_if_changed(filename, new_dict):
    changed = False
    if os.path.isfile(filename):
        with open(filename, 'r') as yml_file:
            old_dict = yaml.safe_load(yml_file)
            if old_dict != new_dict:
               changed = True

    if not os.path.isfile(filename) or changed:
        with open(filename, 'w') as yml_file:
            yaml.dump(new_dict, yml_file)

def render_template_to_string(config, template_filename):
    tpl = get_renderer().get_template(template_filename)
    return tpl.render(config=config)

def _parse_size_bytes(s):
    s = str(s).strip()
    if s.endswith(('K','M','G')):
        num = int(s[:-1], 0) if s[:-1].startswith('0x') else int(s[:-1])
        mul = {'K':1024, 'M':1024*1024, 'G':1024*1024*1024}[s[-1]]
        return num * mul
    return int(s, 0)

def _compat_to_dt(val):
    # Accept string or list; return a DTS-compatible string list
    if isinstance(val, (list, tuple)):
        return ", ".join(f'"{s}"' for s in val)
    return f'"{str(val)}"'  # single string

def build_mem_simple_context(cfg):
    regions = []
    for e in cfg.get('memory', []):
        # Only include entries that explicitly set a compatible
        if 'compatible' not in e:
            continue
        base = int(str(e['offset']), 0)
        size = _parse_size_bytes(e['range'])
        regions.append({
            'name': e['name'],
            'base': base,
            'size': size,
            'compat_str': _compat_to_dt(e['compatible']),
        })
    return {'regions': regions}

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
        return string.replace('K', '*1024U').replace('M', '*1024U*1024U').replace('G', '*1024U*1024U*1024U')

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
    output_filename = sys.argv[2]

    output_dirname = os.path.dirname(output_filename)
    if not os.path.exists(output_dirname):
        os.makedirs(output_dirname)

    config_filename = sys.argv[3]
    config = load_config(config_filename)
    config_path = os.path.dirname(config_filename)

    if sys.version_info[0] < 3:
        reload(sys)
        sys.setdefaultencoding('utf-8')

    elif cmd == '--memory_tcl':
        text = render_template_to_string(append_memory_to_config(config), 'memory.tcl')
        write_if_changed(output_filename, text)

    elif cmd == '--memory_dtsi':
        ctx = build_mem_simple_context(config)
        text = get_renderer().get_template('memory.dtsi').render(**ctx)
        write_if_changed(output_filename, text)

    elif cmd == '--memory_hpp':
        cfg = append_memory_to_config(config)
        cfg['json'] = build_json(cfg)
        text = render_template_to_string(cfg, 'memory.hpp')
        write_if_changed(output_filename, text)

    elif cmd == '--render_template':
        template_filename = sys.argv[4]
        drivers = os.getenv('DRIVERS_HPP','').split()

        server.render_template(
            template_filename,
            output_filename,
            server.get_drivers(drivers)
        )

    elif cmd == '--render_interface':
        driver_filename_hpp = sys.argv[4]
        drivers = os.getenv('DRIVERS_HPP','').split()
        id_ = server.get_driver_id(drivers, driver_filename_hpp)
        server.render_driver(server.get_driver(driver_filename_hpp, id_), output_filename)

    else:
        raise ValueError('make.py unknown command')