#!/usr/bin/env python
# -*- coding: utf-8 -*-

import time
import math
import numpy as np

from koheron_tcp_client import command

class Blink(object):
    def __init__(self, client):
        self.client = client

    @command('BLINK','II')
    def set_dac(self, dac0, dac1):
        pass

    @command('BLINK')
    def get_adc(self):
        return self.recv_tuple('II')