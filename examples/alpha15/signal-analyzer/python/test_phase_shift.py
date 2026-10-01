#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import matplotlib
matplotlib.use('TKAgg')
from matplotlib import pyplot as plt
import os
import pprint

from fft import FFT
from koheron import connect

host = os.getenv('HOST', '192.168.1.115')
client = connect(host, 'signal-analyzer')
pprint.pprint(client.commands)
fft = FFT(client)

fft.set_clock_delay()