#!/bin/bash

ZIP_FILENAME=$1
CURRENT_INSTRUMENT=$2

/bin/rm -rf /tmp/instrument
/bin/mkdir -p /tmp/instrument

/bin/systemctl stop tcp-server.service
/usr/bin/unzip -o ${ZIP_FILENAME} -d /tmp/instrument

echo 'Load bitstream'
/bin/cat /tmp/instrument/${CURRENT_INSTRUMENT}.bit > /dev/xdevcfg
/bin/cat /sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done

echo 'Restart tcp-server'
/bin/cp -f /tmp/instrument/kserverd /usr/local/tcp-server/kserverd
/bin/cp -f /usr/local/flask/tcp_server_local.conf /usr/local/tcp-server/kserver.conf
/bin/systemctl start tcp-server.service

