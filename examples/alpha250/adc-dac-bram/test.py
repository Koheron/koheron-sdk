#!/usr/bin/env python
# -*- coding: utf-8 -*-

import numpy as np
import os
import time
import matplotlib.pyplot as plt
from adc_dac_bram import AdcDacBram
from koheron import connect, Alpha250

host = os.getenv('HOST', '192.168.1.16')
client = connect(host, 'adc-dac-bram', restart=False)
driver = AdcDacBram(client)
alpha = Alpha250(client)

print('ADC size = {}'.format(driver.adc_size))
print('Temperature = {}'.format(alpha.get_temperature()))