#!/bin/bash

set -e

py.test -v tests_instrument_manager.py
py.test -v tests_device_memory.py
