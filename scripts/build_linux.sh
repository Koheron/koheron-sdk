#!/bin/bash
set -e
source scripts/settings.sh
export XILINXD_LICENSE_FILE=/opt/Xilinx/Xilinx.lic

make NAME=blink linux
