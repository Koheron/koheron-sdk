from lase.core import KClient, DevMem
import numpy as np
import matplotlib.pyplot as plt
import time

host = '192.168.1.12'
client = KClient(host)
dvm = DevMem(client)

_config_addr = int('0x60000000',0)
_gpio_addr = int('0x41200000',0)

_led_offset = 0
_pwm_offset = 4

config = dvm.add_memory_map(_config_addr, 1*4096)
gpio = dvm.add_memory_map(_gpio_addr, 16*4096)

dvm.write(gpio, 0, 255)

dvm.write(config, _led_offset, 11)
dvm.write(config, _pwm_offset, 512)
