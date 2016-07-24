#!/bin/bash

set -e

# Lase board
# 192.168.1.5
make NAME=oscillo HOST=192.168.1.5 test_instrum
make NAME=oscillo HOST=192.168.1.5 test_laser
make NAME=oscillo HOST=192.168.1.5 test_eeprom
