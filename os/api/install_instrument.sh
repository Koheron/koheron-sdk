#!/bin/bash

ZIP_FILENAME=$1
CURRENT_INSTRUMENT=$2

/bin/rm -rf /tmp/instrument
/bin/mkdir -p /tmp/instrument

/bin/systemctl stop koheron-server.service
/usr/bin/unzip -o ${ZIP_FILENAME} -d /tmp/instrument

source /tmp/instrument/${CURRENT_INSTRUMENT}.start.sh

echo 'Load bitstream'
/bin/cat /tmp/instrument/${CURRENT_INSTRUMENT}.bit > /dev/xdevcfg
/bin/cat /sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done

echo 'Restart koheron-server'
/bin/systemctl start koheron-server.service