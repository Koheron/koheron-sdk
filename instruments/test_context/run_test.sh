#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

HOST=$HOST py.test -v $DIR/test_memory.py

