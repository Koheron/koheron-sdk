#!/bin/bash

ZIP_FILENAME=$1
CURRENT_INSTRUMENT=$2

/bin/rm -rf /tmp/instrument
/bin/mkdir -p /tmp/instrument

/bin/systemctl stop koheron-server.service
/usr/bin/unzip -o ${ZIP_FILENAME} -d /tmp/instrument

source /tmp/instrument/${CURRENT_INSTRUMENT}.start.sh

cp -Rf /tmp/instrument/static /var/www/ui/live
