#!/usr/bin/env python3

import os
import sys
import json
import jinja2
import yaml
import server

# Import the standalone schema without initializing the Python client package.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'python', 'koheron'))
from memory_schema import parse_int, range_literal, size_bytes

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

def load_config(config_filename):
    ''' Get the config dictionary from the file 'memory.yml' '''

    with open(config_filename) as f:
        config = yaml.safe_load(f)

    return config

def parse_brackets(string):
    ''' ex: 'pwm', '4' = parse_brackets('pwm[4]') '''
    start, end = string.find('['), string.find(']')
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

def _normalize_int_literal(value):
    """Return a string literal without numeric separators."""
    parsed = parse_int(value)
    if parsed is None or parsed < 0:
        raise ValueError(f'Invalid memory offset: {value!r}')
    if isinstance(value, str):
        cleaned = value.replace('_', '')

        lowered = cleaned.lower()
        if lowered.startswith(('0x', '0o')):
            return f'0x{parsed:X}'
        if lowered.startswith('0b'):
            return f'0b{parsed:b}'
        return str(parsed)

    # Preserve hexadecimal formatting for integer literals parsed by YAML.
    return f'0x{parsed:X}'


def build_memory(memory, parameters):
    for address in memory:
        address['name'], address['n_blocks'] = read_parameters(address['name'], parameters)
        assert (address['n_blocks'] > 0)

        if 'offset' in address:
            address['offset'] = _normalize_int_literal(address['offset'])
        address['range'] = range_literal(address['range'])

        # Protection
        if 'protection' not in address:
            address['prot_flag'] = 'PROT_READ|PROT_WRITE'
        elif address['protection'] == 'read':
            address['prot_flag'] = 'PROT_READ'
        elif address['protection'] == 'write':
            address['prot_flag'] = 'PROT_WRITE'

        if 'dev' not in address:
            address['dev'] = '/dev/mem'
        else:
            address['dev'] = str(address['dev'])
        if address['dev'] == '/dev/mem_wc':
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

    return new_registers

def append_memory_to_config(config):
    parameters = config.get('parameters', {})
    config['memory'] = build_memory(config.get('memory', {}), parameters)
    return config

def build_json(dict):
    dict_json = json.dumps(dict, separators=(',', ':')).replace('"', '\\"')
    return dict_json

def render_template_to_string(config, template_filename):
    tpl = get_renderer().get_template(template_filename)
    return tpl.render(config=config)

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
        base = parse_int(e['offset'])
        if base is None or base < 0:
            raise ValueError(f"Invalid memory offset: {e['offset']!r}")
        size = size_bytes(e['range'])
        regions.append({
            'name': e['name'],
            'base': base,
            'size': size,
            'compat_str': _compat_to_dt(e['compatible']),
        })
    arch = os.getenv('ARCH', '').strip()
    return {
        'regions': regions,
        'arch': arch,
        'fclk_rates': {index: cfg['parameters'][f'fclk{index}']
                       for index in range(4) if f'fclk{index}' in cfg.get('parameters', {})},
    }

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

    def replace_KMG(string):
        return f'{size_bytes(string)}U'

    renderer.filters['replace_KMG'] = replace_KMG
    renderer.filters['size_bytes'] = size_bytes
    return renderer

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

    if cmd == '--memory_tcl':
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
