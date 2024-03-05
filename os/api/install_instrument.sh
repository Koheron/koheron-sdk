#!/bin/bash

NAME=$1
LIVE_DIRNAME=$2

/bin/rm -rf ${LIVE_DIRNAME}
/bin/mkdir -p ${LIVE_DIRNAME}

/bin/systemctl stop koheron-server.service
/usr/bin/unzip -o /usr/local/instruments/${NAME}.zip -d ${LIVE_DIRNAME}

# echo 'Load bitstream'
# XDEV=/sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done
# FMAN=/sys/class/fpga_manager/fpga0/firmware
# FFULL=/configfs/device-tree/overlays/full/
# if [ -f "$XDEV" ]; then
#     echo "$XDEV exist"
#     /bin/cat ${LIVE_DIRNAME}/${NAME}.bit > /dev/xdevcfg
#     /bin/cat /sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done
# elif [ -f "$FMAN" ]; then
#     echo "$FMAN exist"
#     mkdir -p /config
#     mkdir -p /lib/firmware
#     mount -t configfs configfs /configfs
#     if [ -d "$FFULL" ]; then
#         echo "$FFULL exist"
#         rmdir $FFULL
#     fi
#     mkdir -p /configfs/device-tree/overlays/full
#     echo 0 > /sys/class/fpga_manager/fpga0/flags
#     cp ${LIVE_DIRNAME}/overlay.dtb /lib/firmware/.
#     cp ${LIVE_DIRNAME}/*.bit.bin /lib/firmware/.
#     echo -n "overlay.dtb" > /configfs/device-tree/overlays/full/path
# else
#     echo "Could not determine fpga manager/xdevcfg"
#     exit -1
# fi

echo 'Restart koheron-server'
/bin/systemctl start koheron-server.service
/bin/systemctl start koheron-server-init.service
