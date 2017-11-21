#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os
import pytest
import struct
import numpy as np
import re

sys.path = [".."] + sys.path
from koheron import connect, command, __version__

class Tests:
    def __init__(self, client):
        self.client = client

    @command(classname='KServer', funcname='get_version')
    def get_server_version(self):
        return self.client.recv_string()

    @command()
    def set_scalars(self, a, b, c, d, e, f):
        return self.client.recv_bool()

    @command()
    def set_array(self, u, f, arr, d, i):
        return self.client.recv_bool()

    @command()
    def get_vector(self):
        return self.client.recv_vector(dtype='float32')

    @command()
    def set_string(self, str):
        return self.client.recv_bool()

    @command()
    def get_string(self):
        return self.client.recv_string()

    @command()
    def get_json(self):
        return self.client.recv_json()

    @command()
    def get_tuple(self):
        return self.client.recv_tuple('Idd?')

# Unit Tests

host = os.getenv('HOST', '192.168.1.100')

client = connect(host, name='test')
tests = Tests(client)

def test_get_server_version():
    server_version = tests.get_server_version()
    server_version_ = server_version.split('.')
    client_version_ = __version__.split('.')
    assert client_version_[0] >= server_version_[0]
    assert client_version_[1] >= server_version_[1]

def test_set_scalars():
    assert tests.set_scalars(429496729, -2048, np.pi, True, np.exp(1), 42)

def test_set_array():
    arr = np.arange(8192, dtype='uint32')
    assert tests.set_array(4223453, np.pi, arr, 2.654798454646, -56789)

def test_get_vector():
    array = tests.get_vector()
    assert len(array) == 10
    for i in range(len(array)):
        assert array[i] == i*i*i

def test_set_string():
    assert tests.set_string('Hello World')

def test_get_string():
    assert tests.get_string() == 'Hello World'

def test_get_json():
    data = tests.get_json()
    assert data['date'] == '20/07/2016'
    assert data['machine'] == 'PC-3'
    assert data['time'] == '18:16:13'
    assert data['user'] == 'thomas'
    assert data['version'] == '0691eed'

def test_get_tuple():
    tup = tests.get_tuple()
    assert tup[0] == 501762438
    assert abs(tup[1] - 507.3858) < 5E-6
    assert abs(tup[2] - 926547.6468507200) < 1E-14
    assert tup[3]