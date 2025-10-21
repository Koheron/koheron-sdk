#!/usr/bin/env python
# -*- coding: utf-8 -*-

# import os
# from pprint import pprint
# from koheron import connect, command

# class App(object):
#     def __init__(self, client):
#         self.client = client

#     @command()
#     def adc_raw_data(self):
#         return self.client.recv_array(2, dtype='int32')

# class ProtoString(object):
#     def __init__(self, client):
#         self.client = client

#     @command()
#     def hello(self, command):
#         return self.client.recv_string()

# host = os.getenv('HOST', '192.168.1.94')
# client = connect(host, 'fft', restart=False)
# pprint(client.commands)
# app = App(client)
# print(app.adc_raw_data())

# proto = ProtoString(client)
# print(proto.hello("Hello"))

import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 16384)
sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
sock.connect(('192.168.1.94', 36000))

def append(buff, value, size):
    if size <= 4:
        for i in reversed(range(size)):
            buff.append((value >> (8 * i)) & 0xff)
    elif size == 8:
        append(buff, value >> 32, 4)
        append(buff, value, 4)

def recv_all(n_bytes):
    '''Receive exactly n_bytes bytes.'''
    data = []
    BUFF_SIZE = 65535
    n_rcv = 0
    while n_rcv < n_bytes:
        try:
            chunk = sock.recv(min(n_bytes - n_rcv, BUFF_SIZE))
            if not chunk:
                raise ConnectionError('recv_all: Socket connection broken.')
            n_rcv += len(chunk)
            data.append(chunk)
        except Exception:
            raise ConnectionError('recv_all: Socket connection broken.')
    return b''.join(data)

def recv_dynamic_payload():
    reserved, class_id, func_id, length = struct.unpack('>IHHI', recv_all(struct.calcsize('>IHHI')))
    assert reserved == 0
    return recv_all(length)

def recv_string():
    return recv_dynamic_payload().decode('utf8')

buff = bytearray()
append(buff, 0, 4)        # RESERVED
append(buff, 0xFFFF, 2)  # driver_id
append(buff, 0, 2)  # op_id
str = "hello!!!!\n"
append(buff, len(str), 4)
buff.extend(str.encode())

for _ in range(10):
    sock.send(buff)
    print(recv_string())

sock.close()