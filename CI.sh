#!/bin/bash
set -e
. settings.sh
export XILINXD_LICENSE_FILE=/opt/Xilinx/Xilinx.lic

make NAME=oscillo
make NAME=spectrum zip
