#!/bin/bash
set -e
source /opt/Xilinx/Vivado/2015.4/settings64.sh
source /opt/Xilinx/SDK/2015.4/settings64.sh
export XILINXD_LICENSE_FILE=/opt/Xilinx/Xilinx.lic
make clean
make NAME=oscillo
make NAME=spectrum zip

