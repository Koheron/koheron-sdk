#!/bin/bash
set -e
source settings.sh
export XILINXD_LICENSE_FILE=/opt/Xilinx/Xilinx.lic

make NAME=oscillo
make NAME=spectrum zip
