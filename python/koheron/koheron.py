#!/usr/bin/env python
# -*- coding: utf-8 -*-

import socket
import struct
import numpy as np
import string
import json
import requests
import time

from .version import __version__

ConnectionError = requests.ConnectionError

# --------------------------------------------
# HTTP API
# --------------------------------------------

def instrument_status(host):
    status = requests.get('http://{}/api/instruments'.format(host)).json()
    return status

def upload_instrument(host, filename, run=False):
    with open(filename, 'rb') as fileobj:
        url = 'http://{}/api/instruments/upload'.format(host)
        r = requests.post(url, files={filename: fileobj})
    if run:
        name = get_name_version(filename)
        r = requests.get('http://{}/api/instruments/run/{}'.format(host, name))

def run_instrument(host, name=None, restart=False):
    instrument_running = False
    instrument_in_store = False
    status = instrument_status(host)
    instruments = status['instruments']
    live_instrument = status['live_instrument']

    if (name is None) or (live_instrument == name): # Instrument already running
        name = live_instrument
        instrument_running = True

    if not instrument_running: # Find the instrument in the local store:
        if name in instruments:
            instrument_in_store = True
	else:
            raise ValueError('Did not found instrument {}'.format(name))

    if instrument_in_store or (instrument_running and restart):
        r = requests.get('http://{}/api/instruments/run/{}'.format(host, name))

def connect(host, *args, **kwargs):
    run_instrument(host, *args, **kwargs)
    client = KoheronClient(host)
    return client

def load_instrument(host, instrument='blink', always_restart=False):
    print('Warning: load_instrument() is deprecated, use connect() instead')
    run_instrument(host, instrument, restart=always_restart)
    client = KoheronClient(host)
    return client

# --------------------------------------------
# Command decorator
# --------------------------------------------

def command(classname=None, funcname=None):
    def real_command(func):
        def wrapper(self, *args):
            device_name = classname or self.__class__.__name__
            cmd_name = funcname or func.__name__
            device_id, cmd_id, cmd_args = self.client.get_ids(device_name, cmd_name)
            self.client.send_command(device_id, cmd_id, cmd_args, *args)
            self.client.last_device_called = device_name
            self.client.last_cmd_called = cmd_name
            return func(self, *args)
        return wrapper
    return real_command

# --------------------------------------------
# Helper functions
# --------------------------------------------

def make_command(*args):
    buff = bytearray()
    append(buff, 0, 4)        # RESERVED
    append(buff, args[0], 2)  # driver_id
    append(buff, args[1], 2)  # op_id
    # Payload
    if len(args[2:]) > 0:
        buff += build_payload(args[2], args[3:])
    else:
        append(buff, 0, 8)
    return buff

def append(buff, value, size):
    if size <= 4:
        for i in reversed(range(size)):
            buff.append((value >> (8 * i)) & 0xff)
    elif size == 8:
        append(buff, value >> 32, 4)
        append(buff, value, 4)

def append_vector(buff, array, array_params):
    if cpp_to_np_types[array_params['T']] != array.dtype:
        raise TypeError('Invalid array type. Expected {} but received {}.'
                        .format(cpp_to_np_types[array_params['T']], array.dtype))

    arr_bytes = bytearray(array)
    append(buff, len(arr_bytes), 4)
    buff += arr_bytes

def append_array(buff, array, array_params):
    if int(array_params['N']) != len(array):
        raise ValueError('Invalid array length. Expected {} but received {}.'
                         .format(array_params['N'], len(array)))

    if cpp_to_np_types[array_params['T']] != array.dtype:
        raise TypeError('Invalid array type. Expected {} but received {}.'
                        .format(cpp_to_np_types[array_params['T']], array.dtype))

    buff += bytearray(array)

# http://stackoverflow.com/questions/14431170/get-the-bits-of-a-float-in-python
def float_to_bits(f):
    return struct.unpack('>l', struct.pack('>f', f))[0]

def double_to_bits(d):
    return struct.unpack('>q', struct.pack('>d', d))[0]

def build_payload(cmd_args, args):
    payload = bytearray()

    if len(cmd_args) != len(args):
        raise ValueError('Invalid number of arguments. Expected {} but received {}.'
                         .format(len(cmd_args), len(args)))

    if len(cmd_args) == 0:
        return payload

    for i, arg in enumerate(cmd_args):
        if arg['type'] in ['uint8_t','int8_t']:
            append(payload, args[i], 1)
        elif arg['type'] in ['uint16_t','int16_t']:
            append(payload, args[i], 2)
        elif arg['type'] in ['uint32_t','int32_t']:
            append(payload, args[i], 4)
        elif arg['type'] in ['uint64_t','int64_t']:
            append(payload, args[i], 8)
        elif arg['type'] == 'float':
            append(payload, float_to_bits(args[i]), 4)
        elif arg['type'] == 'double':
            append(payload, double_to_bits(args[i]), 8)
        elif arg['type'] == 'bool':
            if args[i]:
                append(payload, 1, 1)
            else:
                append(payload, 0, 1)
        elif is_std_array(arg['type']):
            append_array(payload, args[i], get_std_array_params(arg['type']))
        elif is_std_vector(arg['type']):
            append_vector(payload, args[i], get_std_vector_params(arg['type']))
        elif is_std_string(arg['type']):
            append(payload, len(args[i]), 4)
            payload.extend(args[i].encode())
        else:
            raise ValueError('Unsupported type "' + arg['type'] + '"')

    return payload

def is_std_array(_type):
    return _type.split('<')[0].strip() == 'std::array'

def is_std_vector(_type):
    return _type.split('<')[0].strip() == 'std::vector'

def is_std_string(_type):
    return _type.strip() == 'std::string'

def is_std_tuple(_type):
    return _type.split('<')[0].strip() == 'std::tuple'

def get_std_array_params(_type):
    templates = _type.split('<')[1].split('>')[0].split(',')
    return {
      'T': templates[0].strip(),
      'N': templates[1].split('u')[0].strip()
    }

def get_std_vector_params(_type):
    return {'T': _type.split('<')[1].split('>')[0].strip()}

cpp_to_np_types = {
  'bool': 'bool',
  'uint8_t': 'uint8', 'int8_t': 'int8',
  'uint16_t': 'uint16', 'int16_t': 'int16',
  'uint32_t': 'uint32', 'unsigned int': 'uint32',
  'int32_t': 'int32', 'int': 'int32',
  'uint64_t': 'uint64', 'int64_t': 'int64',
  'float': 'float32',
  'double': 'float64'
}

# --------------------------------------------
# KoheronClient
# --------------------------------------------

class KoheronClient:
    def __init__(self, host='', port=36000, unixsock=''):
        ''' Initialize connection with koheron-server

        Args:
            host: A string with the IP address
            port: Port of the TCP connection (must be an integer)
        '''
        if type(host) != str:
            raise TypeError('IP address must be a string')

        if type(port) != int:
            raise TypeError('Port number must be an integer')

        self.host = host
        self.port = port
        self.unixsock = unixsock
        self.is_connected = False

        if host != '':
            try:
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

                # Prevent delayed ACK on Ubuntu
                self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 16384)
                so_rcvbuf = self.sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)

                #   Disable Nagle algorithm for real-time response:
                self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                tcp_nodelay = self.sock.getsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY)
                assert tcp_nodelay == 1

                # Connect to Kserver
                self.sock.connect((host, port))
                self.is_connected = True
            except BaseException as e:
                raise ConnectionError('Failed to connect to {}:{} : {}'.format(host, port, e))
        elif unixsock != '':
            try:
                self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.sock.connect(unixsock)
                self.is_connected = True
            except BaseException as e:
                raise ConnectionError('Failed to connect to unix socket address ' + unixsock)
        else:
            raise ValueError('Unknown socket type')

        if self.is_connected:
            self.check_version()
            self.load_devices()

    def check_version(self):
        try:
            self.send_command(1, 0)
        except:
            raise ConnectionError('Failed to retrieve the server version')
        server_version = self.recv_string(check_type=False)
        server_version_ = server_version.split('.')
        client_version_ = __version__.split('.')
        if  (client_version_[0] != server_version_[0]) or (client_version_[1] < server_version_[1]):
            print('Warning: your client version {} is incompatible with the server version {}'
                   .format(__version__, server_version))
            print('Upgrade your client with "pip install --upgrade koheron"')

    def load_devices(self):
        try:
            self.send_command(1, 1)
        except:
            raise ConnectionError('Failed to send initialization command')

        self.commands = self.recv_json(check_type=False)
        # pprint.pprint(self.commands)
        self.devices_idx = {}
        self.cmds_idx_list = [None]*(2 + len(self.commands))
        self.cmds_args_list = [None]*(2 + len(self.commands))
        self.cmds_ret_types_list = [None]*(2 + len(self.commands))

        for device in self.commands:
            self.devices_idx[device['class']] = device['id']
            cmds_idx = {}
            cmds_args = {}
            cmds_ret_type = {}
            for cmd in device['functions']:
                cmds_idx[cmd['name']] = cmd['id']
                cmds_args[cmd['name']] = cmd['args']
                cmds_ret_type[cmd['name']] = cmd.get('ret_type', None)
            self.cmds_idx_list[device['id']] = cmds_idx
            self.cmds_args_list[device['id']] = cmds_args
            self.cmds_ret_types_list[device['id']] = cmds_ret_type

    def get_ids(self, device_name, command_name):
        device_id = self.devices_idx[device_name]
        cmd_id = self.cmds_idx_list[device_id][command_name]
        cmd_args = self.cmds_args_list[device_id][command_name]
        return device_id, cmd_id, cmd_args

    def check_ret_type(self, expected_types):
        device_id = self.devices_idx[self.last_device_called]
        ret_type = self.cmds_ret_types_list[device_id][self.last_cmd_called]
        if ret_type not in expected_types:
            raise TypeError('{}::{} returns a {}.'.format(self.last_device_called, self.last_cmd_called, ret_type))

    def check_ret_array(self, dtype, arr_len):
        device_id = self.devices_idx[self.last_device_called]
        ret_type = self.cmds_ret_types_list[device_id][self.last_cmd_called]
        if not is_std_array(ret_type):
            raise TypeError('Expect call to rcv_array [{}::{} returns a {}].'.format(self.last_device_called, self.last_cmd_called, ret_type))
        params = get_std_array_params(ret_type)
        if dtype != cpp_to_np_types[params['T']]:
            raise TypeError('{}::{} expects elements of type {}.'.format(self.last_device_called, self.last_cmd_called, params['T']))
        if arr_len != int(params['N']):
            raise ValueError('{}::{} expects {} elements.'.format(self.last_device_called, self.last_cmd_called, params['N']))

    def check_ret_vector(self, dtype):
        device_id = self.devices_idx[self.last_device_called]
        ret_type = self.cmds_ret_types_list[device_id][self.last_cmd_called]
        if not is_std_vector(ret_type):
            raise TypeError('Expect call to rcv_vector [{}::{} returns a {}].'.format(self.last_device_called, self.last_cmd_called, ret_type))
        vect_type = get_std_vector_params(ret_type)['T']
        if dtype != cpp_to_np_types[vect_type]:
            raise TypeError('{}::{} expects elements of type {}.'.format(self.last_device_called, self.last_cmd_called, vect_type))

    # TODO add types check
    def check_ret_tuple(self):
        device_id = self.devices_idx[self.last_device_called]
        ret_type = self.cmds_ret_types_list[device_id][self.last_cmd_called]
        if not is_std_tuple(ret_type):
            raise TypeError('{}::{} returns a {} not a std::tuple.'.format(self.last_device_called, self.last_cmd_called, ret_type))

    # -------------------------------------------------------
    # Send/Receive
    # -------------------------------------------------------

    def send_command(self, device_id, cmd_id, cmd_args=[], *args):
        cmd = make_command(device_id, cmd_id, cmd_args, *args)
        if self.sock.send(cmd) == 0:
            raise ConnectionError('send_command: Socket connection broken')

    def recv_all(self, n_bytes):
        '''Receive exactly n_bytes bytes.'''
        data = []
        n_rcv = 0
        while n_rcv < n_bytes:
            try:
                chunk = self.sock.recv(n_bytes - n_rcv)
                if chunk == '':
                    break
                n_rcv += len(chunk)
                data.append(chunk)
            except:
                raise ConnectionError('recv_all: Socket connection broken.')
        return b''.join(data)

    def recv_dynamic_payload(self):
        reserved, class_id, func_id, length = struct.unpack('>IHHI', self.recv_all(struct.calcsize('>IHHI')))
        assert reserved == 0
        return self.recv_all(length)

    def recv(self, fmt='I'):
        fmt_ = '>IHH' + fmt
        t = struct.unpack(fmt_, self.recv_all(struct.calcsize(fmt_)))[3:]
        if len(t) == 1:
            return t[0]
        else:
            return t

    def recv_uint32(self):
        self.check_ret_type(['uint32_t', 'unsigned int'])
        return self.recv()

    def recv_uint64(self):
        self.check_ret_type(['uint64_t', 'unsigned long'])
        return self.recv(fmt='Q')

    def recv_int32(self):
        self.check_ret_type(['int32_t', 'int'])
        return self.recv(fmt='i')

    def recv_float(self):
        self.check_ret_type(['float'])
        return self.recv(fmt='f')

    def recv_double(self):
        self.check_ret_type(['double'])
        return self.recv(fmt='d')

    def recv_bool(self):
        self.check_ret_type(['bool'])
        return self.recv(fmt='?')

    def recv_string(self, check_type=True):
        if check_type:
            self.check_ret_type(['std::string', 'const char *', 'const char*'])
        return self.recv_dynamic_payload().decode('utf8')

    def recv_json(self, check_type=True):
        if check_type:
            self.check_ret_type(['std::string', 'const char *', 'const char*'])
        return json.loads(self.recv_string(check_type=False))

    def recv_vector(self, dtype='uint32', check_type=True):
        '''Receive a numpy array with unknown length.'''
        if check_type:
            self.check_ret_vector(dtype)
        dtype = np.dtype(dtype)
        buff = self.recv_dynamic_payload()
        return np.frombuffer(buff, dtype=dtype.newbyteorder('<'))

    def recv_array(self, shape, dtype='uint32', check_type=True):
        '''Receive a numpy array with known shape.'''
        arr_len = int(np.prod(shape))
        if check_type:
            self.check_ret_array(dtype, arr_len)
        dtype = np.dtype(dtype)
        self.recv(fmt='')
        buff = self.recv_all(dtype.itemsize * arr_len)
        return np.frombuffer(buff, dtype=dtype.newbyteorder('<')).reshape(shape)

    def recv_tuple(self, fmt, check_type=True):
        if check_type:
            self.check_ret_tuple()
        return tuple(self.recv(fmt))

    def __del__(self):
        if hasattr(self, 'sock'):
            self.sock.close()
