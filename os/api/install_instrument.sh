#!/bin/bash

NAME=$1

/bin/rm -rf /tmp/live-instrument
/bin/mkdir -p /tmp/live-instrument

/bin/systemctl stop koheron-server.service
/usr/bin/unzip -o /usr/local/instruments/${NAME}.zip -d /tmp/live-instrument

source /tmp/live-instrument/start.sh

echo 'Load bitstream'
/bin/cat /tmp/live-instrument/${NAME}.bit > /dev/xdevcfg
/bin/cat /sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done

echo 'Restart koheron-server'
/bin/systemctl start koheron-server.service
/bin/systemctl start koheron-server-init.service