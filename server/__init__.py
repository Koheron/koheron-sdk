# (c) Koheron

import os
import re
import CppHeaderParser
import jinja2
import json
import sys
import yaml

SDK_PATH = os.getenv('SDK_PATH', '')

# -----------------------------------------------------------------------------------------
# Code generation
# -----------------------------------------------------------------------------------------

def get_driver(path, driver_id=0):
    driver = Driver(path)
    driver.id = driver_id
    driver.calls = cmd_calls(driver.raw, driver_id)
    return driver

def get_driver_id(drivers_list, driver_path):
    drivers_ids ={}
    driver_id = 2
    for path in drivers_list or []:
        assert(path.endswith('.hpp') or path.endswith('.h'))
        dir1, file1 = os.path.split(path)
        dir2, file2 = os.path.split(driver_path)
        if file1 == file2:
                return driver_id
        driver_id +=1
    return None

def get_drivers(drivers_list):
    drivers = [] # List of generated drivers
    obj_files = []  # Object file names
    driver_id = 2
    for path in drivers_list or []:
        assert(path.endswith('.hpp') or path.endswith('.h'))
        driver = get_driver(path, driver_id)
        driver_id +=1
        drivers.append(driver)
    return drivers

class Driver:
    def __init__(self, path, base_dir='.'):
        dev = parse_header(os.path.join(base_dir, path))[0]
        self.raw = dev
        self.header_path = os.path.dirname(path)
        self.path = path
        self.operations = dev['operations']
        self.tag = dev['tag']
        self.name = dev['name']
        self.class_name = 'InTerface' + self.tag.capitalize()
        self.objects = dev['objects']
        self.includes = dev['includes']
        self.interface_name = 'interface_' + os.path.basename(self.includes[0]).split('.')[0]
        self.id = None
        self.calls = None

def get_json(drivers):
    data = [{
        'class': 'KServer',
        'id': 1,
        'functions': [
            {'name': 'get_version', 'id': 0, 'args': [], 'ret_type': 'const char *'},
            {'name': 'get_cmds', 'id': 1, 'args': [], 'ret_type': 'std::string'}
        ]
    }]

    for driver in drivers:
        data.append({
            'class': driver.name,
            'id': driver.id,
            'functions': [
                {'name': op['name'], 'id': op['id'], 'ret_type': format_ret_type(driver.name, op),'args': op.get('args_client',[])}
                for op in driver.operations
            ]
        })

    return json.dumps(data, separators=(',', ':')).replace('"', '\\"').replace('\\\\','')

def get_template(filename):
    renderer = jinja2.Environment(
      block_start_string = '{%',
      block_end_string = '%}',
      variable_start_string = '{{',
      variable_end_string = '}}',
      loader = jinja2.FileSystemLoader(os.path.join(SDK_PATH, 'server/templates'))
    )

    def get_fragment(operation, driver):
        return driver.calls[operation['tag']]

    def get_parser(operation, driver):
        return parser_generator(driver, operation)

    renderer.filters['get_fragment'] = get_fragment
    renderer.filters['get_parser'] = get_parser
    renderer.filters['get_exact_ret_type'] = get_exact_ret_type

    return renderer.get_template(filename)

def render_template(template_filename, output_filename, drivers):
    with open(output_filename, 'w') as output:
        output.write(get_template(os.path.basename(template_filename)).render(drivers=drivers, json=get_json(drivers)))

def render_driver(driver, output_filename_hpp):
    output_filename_split = os.path.splitext(output_filename_hpp)
    assert(output_filename_split[1] == '.hpp')
    for extension in ['.cpp', '.hpp']:
        with open(output_filename_split[0] + extension, 'w') as output:
            output.write(get_template('interface_driver' + extension).render(driver=driver))

# -----------------------------------------------------------------------------
# Parse driver C++ header
# -----------------------------------------------------------------------------

def parse_header(hppfile):
    cpp_header = CppHeaderParser.CppHeader(hppfile)
    drivers = []
    for classname in cpp_header.classes:
        drivers.append(parse_driver_header(cpp_header.classes[classname], hppfile))
    return drivers

def parse_driver_header(_class, hppfile):
    driver = {}
    driver['name'] = _class['name']
    driver['tag'] = '_'.join(re.findall('[A-Z][^A-Z]*', driver['name'])).upper()
    driver['includes'] = [hppfile]
    driver['objects'] = [{
      'type': str(_class['name']),
      'name': '__' + _class['name']
    }]

    driver['operations'] = []
    op_id = 0
    for method in _class['methods']['public']:
        # We eliminate constructor, destructor and templates
        if (not (method['name'] in [s + _class['name'] for s in ['','~']])) and not method['template']:
            driver['operations'].append(parse_header_operation(driver['name'], method))
            driver['operations'][-1]['id'] = op_id
            op_id += 1
    return driver

def parse_header_operation(driver_name, method):
    operation = {}
    operation['tag'] = method['name'].upper()
    operation['name'] = method['name']
    operation['ret_type'] = method['rtnType']

    check_type(operation['ret_type'], driver_name, operation['name'])

    if len(method['parameters']) > 0:
        operation['arguments'] = [] # Use for code generation
        operation['args_client'] = [] # Send to client
        for param in method['parameters']:
            arg = {}
            arg['name'] = str(param['name'])
            arg['type'] = param['type'].strip()

            if arg['type'][-1:] == '&': # Argument passed by reference
                arg['by_reference'] = True
                arg['type'] = arg['type'][:-2].strip()
            if arg['type'][:5] == 'const':# Argument is const
                arg['is_const'] = True
                arg['type'] = arg['type'][5:].strip()

            check_type(arg['type'], driver_name, operation['name'])
            operation['arguments'].append(arg)
            operation['args_client'].append({'name': arg['name'], 'type': format_type(arg['type'])})
    return operation

# The following integers are forbiden since they are plateform
# dependent and thus not compatible with network use.
FORBIDDEN_INTS = ['short', 'int', 'unsigned', 'long', 'unsigned short', 'short unsigned',
                  'unsigned int', 'int unsigned', 'unsigned long', 'long unsigned',
                  'long long', 'unsigned long long', 'long long unsigned']

def check_type(_type, driver_name, opname):
    if _type in FORBIDDEN_INTS:
        raise ValueError('[{}::{}] Invalid type "{}": Only integers with exact width (e.g. uint32_t) are supported (http://en.cppreference.com/w/cpp/header/cstdint).'.format(driver_name, opname, _type))

def format_type(_type):
    if is_std_array(_type):
        templates = _type.split('<')[1].split('>')[0].split(',')
        return 'std::array<{}, " << {} << ">'.format(templates[0], templates[1])
    else:
        return _type

def get_exact_ret_type(classname, operation):
    if 'auto' in operation['ret_type'] or is_std_array(operation['ret_type']):
        decl_arg_list = []
        for arg in operation.get('arguments', []):
            decl_arg_list.append('std::declval<{}>()'.format(arg['type']))
        return 'decltype(std::declval<{}>().{}({}))'.format(classname, operation['name'], ' ,'.join(decl_arg_list))
    else:
        return operation['ret_type']

def format_ret_type(classname, operation):
    if 'auto' in operation['ret_type'] or is_std_array(operation['ret_type']):
        return '" << get_type_str<{}>() << "'.format(get_exact_ret_type(classname, operation))
    else:
        return operation['ret_type']


# -----------------------------------------------------------------------------
# Generate command call and send
# -----------------------------------------------------------------------------

def cmd_calls(driver, driver_id):
    calls = {}
    for op in driver['operations']:
        calls[op['tag']] = generate_call(driver, driver_id, op)
    return calls

def generate_call(driver, driver_id, operation):
    def build_func_call(driver, operation):
        call = driver['objects'][0]['name'] + '.' + operation['name'] + '('
        call += ', '.join('args_' + operation['name'] + '.' + arg['name'] for arg in operation.get('arguments', []))
        return call + ')'

    lines = []
    if operation['ret_type'] == 'void':
        lines.append('    {};\n'.format(build_func_call(driver, operation)))
        lines.append('    return 0;\n')
    else:
        lines.append('    return cmd.session->send<{}, {}>({});\n'.format(driver_id, operation['id'], build_func_call(driver, operation)))
    return ''.join(lines)

# -----------------------------------------------------------
# Parse command arguments
# -----------------------------------------------------------

def parser_generator(driver, operation):

    if operation.get('arguments') is None:
        return ''

    lines = []
    packs, has_vector = build_args_packs(lines, operation)

    if not has_vector:
        print_required_buff_size(lines, packs)
        lines.append('    static_assert(req_buff_size <= cmd.payload.size(), "Buffer size too small");\n\n');

    for idx, pack in enumerate(packs):
        if pack['family'] == 'scalar':
            lines.append('\n    auto args_tuple' + str(idx)  + ' = cmd.session->deserialize<')
            print_type_list_pack(lines, pack)
            lines.append('>(cmd);\n')
            lines.append('    if (std::get<0>(args_tuple' + str(idx)  + ') < 0) {\n')
            lines.append('        return -1;\n')
            lines.append('    }\n')

            for i, arg in enumerate(pack['args']):
                lines.append('    args_' + operation['name'] + '.' + arg["name"] + ' = ' + 'std::get<' + str(i + 1) + '>(args_tuple' + str(idx) + ');\n');

        elif pack['family'] in ['vector', 'string', 'array']:
            lines.append('    if (cmd.session->recv(args_' + operation['name'] + '.' + pack['args']['name'] + ', cmd) < 0) {\n')
            lines.append('        return -1;\n')
            lines.append('    }\n\n')
        else:
            raise ValueError('Unknown argument family')
    return ''.join(lines)

def print_required_buff_size(lines, packs):
    lines.append('    constexpr size_t req_buff_size = ')

    for idx, pack in enumerate(packs):
        if pack['family'] == 'scalar':
            if idx == 0:
                lines.append('required_buffer_size<')
            else:
                lines.append('                                     + required_buffer_size<')
            print_type_list_pack(lines, pack)
            if idx < len(packs) - 1:
                lines.append('>()\n')
            else:
                lines.append('>();\n')
        elif pack['family'] == 'array':
            array_params = get_std_array_params(pack['args']['type'])
            if idx == 0:
                lines.append('size_of<')
            else:
                lines.append('                                     + size_of<')
            lines.append(array_params['T'] + ', ' + array_params['N'] + '>')
            if idx < len(packs) - 1:
                lines.append('\n')
            else:
                lines.append(';\n')

def print_type_list_pack(lines, pack):
    for idx, arg in enumerate(pack['args']):
        if idx < len(pack['args']) - 1:
            lines.append(arg['type'] + ', ')
        else:
            lines.append(arg['type'])

def build_args_packs(lines, operation):
    ''' Packs the adjacent scalars together for deserialization
        and separate them from the arrays and vectors '''
    packs = []
    args_list = []
    has_vector = False
    for idx, arg in enumerate(operation["arguments"]):
        if is_std_array(arg['type']):
            if len(args_list) > 0:
                packs.append({'family': 'scalar', 'args': args_list})
                args_list = []
            packs.append({'family': 'array', 'args': arg})
        elif is_std_vector(arg['type']) or is_std_string(arg['type']):
            has_vector = True
            if len(args_list) > 0:
                packs.append({'family': 'scalar', 'args': args_list})
                args_list = []
            if is_std_vector(arg['type']):
                packs.append({'family': 'vector', 'args': arg})
            elif is_std_string(arg['type']):
                packs.append({'family': 'string', 'args': arg})
            else:
                assert False
        else:
            args_list.append(arg)
    if len(args_list) > 0:
        packs.append({'family': 'scalar', 'args': args_list})
    return packs, has_vector

def is_std_array(arg_type):
    container_type = arg_type.split('<')[0].strip()
    return  container_type in ['std::array', 'const std::array']

def is_std_vector(arg_type):
    container_type = arg_type.split('<')[0].strip()
    return  container_type in ['std::vector', 'const std::vector']

def is_std_string(arg_type):
    return arg_type.strip() in ['std::string', 'const std::string']

def get_std_array_params(arg_type):
    templates = arg_type.split('<')[1].split('>')[0].split(',')
    return {
      'T': templates[0].strip(),
      'N': templates[1].strip()
    }
