#!/bin/bash
set -e

# Laser board
host=192.168.1.5
name=oscillo

make HOST=$host app_sync
sleep 3
make NAME=$name HOST=$host test_instrument_manager
make NAME=$name HOST=$host test_instrum
make NAME=$name HOST=$host test_laser
make NAME=$name HOST=$host test_eeprom
