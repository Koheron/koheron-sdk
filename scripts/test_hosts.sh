#!/bin/bash

set -e

# Lase board
# 192.168.1.5
make HOST=192.168.1.5 app_sync
sleep 3
make NAME=oscillo HOST=192.168.1.5 test_instrument_manager
make NAME=oscillo HOST=192.168.1.5 test_instrum
make NAME=oscillo HOST=192.168.1.5 test_laser
make NAME=oscillo HOST=192.168.1.5 test_eeprom
