#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import jinja2
import yaml
import sys
import time
import socket
import getpass
import json
import requests
import zipfile
import StringIO

def format_paths(paths, instrument_path):
    ''' If a path starts with './' then it is relative to the instrument path,
        that is the folder containing the ins instrument 'config.yml'
    '''
    fmt_paths = []
    for path in paths:
        if path.startswith('./'):
            fmt_paths.append(os.path.join(instrument_path, path))
        else:
            fmt_paths.append(path)
    return fmt_paths

def get_parent_path(parent_filename):
    if parent_filename == '<default>':
        return 'instruments/default'
    else:
        return parent_filename

def get_list(prop, instrument_path, prop_list=None):
    ''' Ex: Get the list of cores needed by the 'oscillo' instrument.
        list = get_list('cores', 'instruments/oscillo')
    '''
    if prop_list is None: 
       prop_list = []
    config = load_config(instrument_path)
    if 'parent' in config and config['parent'] != None:
        prop_list.extend(get_list(prop, get_parent_path(config['parent']), prop_list))
    if prop in config:
        prop_list.extend(config[prop])
    # Ensure each item is only included once:
    prop_list = list(set(prop_list))
    return prop_list

def get_prop(prop, instrument_path):
    config = load_config(instrument_path)
    if not prop in config:
        if 'parent' in config:
            config[prop] = get_prop(prop, get_parent_path(config['parent']))
        else:
            return None
    return config[prop]

def load_config(instrument_path):
    ''' Get the config dictionary from the file 'config.yml' '''
    config_filename = os.path.join(instrument_path, 'config.yml')
    with open(config_filename) as f:
        cfg = yaml.load(f)

    if 'parent' in cfg:
        cfg['parent'] = format_paths([cfg['parent']], instrument_path)[0]

    for field in ['modules', 'cores', 'xdc']:
        if field in cfg:
            cfg[field] = format_paths(cfg[field], instrument_path)

    if 'server' in cfg:
        if 'includes' in cfg['server']:
            cfg['server']['includes'] = format_paths(cfg['server']['includes'], instrument_path)
        if 'drivers' in cfg['server']:
            cfg['server']['drivers'] = format_paths(cfg['server']['drivers'], instrument_path)

    return cfg

def parse_brackets(string):
    ''' ex: 'pwm', '4' = parse_brackets('pwm[4]') '''
    start, end = map(lambda char : string.find(char), ('[',']'))
    if start >= 0 and end >= 0:
        return string[0:start], string[start+1:end]
    else:
        return string, '1'

def get_config(instrument_path):
    ''' Build the config dictionary recursively.
        ex: config = get_config('instruments/oscillo')
    '''
    cfg = load_config(instrument_path)

    # Get missing elements from ancestors
    lists = ['cores', 'xdc', 'modules']
    for list_ in lists:
        cfg[list_] = get_list(list_, instrument_path)
    props = ['board', 'live_zip']
    for prop in props:
        cfg[prop] = get_prop(prop, instrument_path)

    params = cfg.get('parameters', {})

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
    lists = ['config_registers', 'status_registers']
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
        module_cfg = get_config(module)
        cfg['cores'].extend(module_cfg['cores'])
        cfg['cores'] = list(set(cfg['cores']))
    
    # SHA
    sha_filename = os.path.join('tmp', cfg['instrument'] + '.sha')
    if os.path.isfile(sha_filename):
        with open(sha_filename) as sha_file:
            sha = sha_file.read()
            for i in range(8):
                cfg['parameters']['sha' + str(i)] = int('0x' + sha[8*i:8*i+8], 0)

    cfg['json'] = json.dumps(cfg, separators=(',', ':')).replace('"', '\\"')
    return cfg

def dump_if_has_changed(filename, new_dict):
    has_changed = False
    if os.path.isfile(filename):
        with open(filename, 'r') as yml_file:
            old_dict = yaml.load(yml_file)
            if old_dict != new_dict:
               has_changed = True

    if not os.path.isfile(filename) or has_changed:
        with open(filename, 'w') as yml_file:
            yaml.dump(new_dict, yml_file)

###################
# Jinja
###################

def fill_template(config, template_filename, output_filename):
    template = get_renderer().get_template(os.path.join('scripts/templates', template_filename))
    with open(output_filename, 'w') as output:
        output.write(template.render(dic=config))

def fill_config_tcl(config):
    output_filename = os.path.join('tmp', config['instrument'] + '.config.tcl')
    fill_template(config, 'config.tcl', output_filename)

def fill_memory(config, drivers_dir):
    output_filename = os.path.join(drivers_dir, 'memory.hpp')
    fill_template(config, 'memory.hpp', output_filename)

def fill_start_sh(config):
    output_filename = os.path.join('tmp', config['instrument'] + '.start.sh')
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
        return string.replace('K', '*1024').replace('M', '*1024*1024').replace('G', '*1024*1024*1024')
    renderer.filters['quote'] = quote
    renderer.filters['remove_extension'] = remove_extension
    renderer.filters['replace_KMG'] = replace_KMG

    return renderer

###################
# Test
###################

# Remove numbers from string
strip_num = lambda string: ''.join([char for char in string if char not in '0123456789'])

def test_module_consistency(instrument):
    ''' Check that the modules registers are defined in the instrument config.yml '''
    cfg = get_config(instrument)
    props = ['config_registers', 'status_registers']
    for module in cfg['modules']:
        module_cfg = get_config(module)
        for prop in props:
            a = module_cfg[prop]
            a = a if a is not None else []
            b = map(strip_num, set(cfg[prop]))
            assert set(a).issubset(b)

def test_core_consistency(instrument):
    ''' Check that the modules cores are defined in the instrument config.yml '''
    cfg = get_config(instrument)
    for module in cfg['modules']:
        module_cfg = get_config(module)
        assert set(module_cfg['cores']).issubset(cfg['cores'])

def print_config(instrument):
    cfg = get_config(instrument)
    print('INSTRUMENT = {}'.format(cfg['instrument']))
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
        for instrument in sys.argv[2:]:
            print_config(instrument)
            test_module_consistency(instrument)
            test_core_consistency(instrument)

    elif cmd == '--split_config_yml':
        cfg = load_config(sys.argv[2])
        dump_if_has_changed(os.path.join('tmp', cfg['instrument'] + '.drivers.yml'), cfg.get('server'))

        cfg.pop('server', None)
        dump_if_has_changed(os.path.join('tmp', cfg['instrument'] + '.config.yml'), cfg)

    elif cmd == '--config_tcl':
        config = get_config(sys.argv[2])
        assert('sha0' in config['parameters'])
        fill_config_tcl(config)

    elif cmd == '--start_sh':
        fill_start_sh(get_config(sys.argv[2]))

    elif cmd == '--cores':
        config = get_config(sys.argv[2])
        cores_filename = os.path.join('tmp', config['instrument'] + '.cores')
        with open(cores_filename, 'w') as f:
            f.write(' '.join(config['cores']))

    elif cmd == '--board':
        config = get_config(sys.argv[2])
        board_filename = os.path.join('tmp', config['instrument'] + '.board')
        with open(board_filename, 'w') as f:
            f.write(config['board'])

    elif cmd == '--drivers':
        cfg = load_config(sys.argv[2])
        drivers = cfg.get('server', {})

        for include_filename in drivers.get('includes', []):
            with open(include_filename) as include_file:
                inc_dict = yaml.load(include_file)
                if 'drivers' in inc_dict:
                    inc_dict['drivers'] = format_paths(inc_dict['drivers'],
                                            os.path.dirname(include_filename))
                for key, value in inc_dict.iteritems():
                    drivers.get(key, []).extend(value)

        with open(os.path.join('tmp', cfg['instrument'] + '.drivers'), 'w') as f:
            f.write((' '.join(drivers['drivers'])) if ('drivers' in drivers) else '')

    elif cmd == '--xdc':
        config = get_config(sys.argv[2])
        xdc_filename = os.path.join('tmp', config['instrument'] + '.xdc')
        with open(xdc_filename, 'w') as f:
            f.write(' '.join(config['xdc']))

    elif cmd == '--live_zip':
        version = sys.argv[3]
        s3_url = sys.argv[4]
        config = get_config(sys.argv[2])
        zip_url = '{}/{}-{}'.format(s3_url, version, config['live_zip'])
        r = requests.get(zip_url, stream=True)
        z = zipfile.ZipFile(StringIO.StringIO(r.content))
        z.extractall('tmp/%s.live' % config['instrument'])

    elif cmd == '--middleware':
        config = get_config(sys.argv[2])
        dest =  'tmp/' + config['instrument'] + '.server.build'
        if not os.path.exists(dest):
            os.makedirs(dest)
        fill_memory(config, dest)

    # -- HTTP API

    elif cmd == '--metadata':
        metadata = {
          'version': sys.argv[2],
          'date': time.strftime("%d/%m/%Y"),
          'time': time.strftime("%H:%M:%S"),
          'machine': socket.gethostname(),
          'user': getpass.getuser()
        }

        with open('tmp/metadata.json', 'w') as f:
            json.dump(metadata, f)

    else:
        raise ValueError('Unknown command')
