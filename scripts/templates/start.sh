#!/bin/bash

echo 'Clock initialization...'
/bin/bash /tmp/instrument/start.sh

devcfg=/sys/devices/soc0/amba/f8007000.devcfg

clk0 = 200000000
test -d $devcfg/fclk/fclk0 || echo fclk0 > $devcfg/fclk_export
echo 1 > $devcfg/fclk/fclk0/enable
echo $clk0 > $devcfg/fclk/fclk0/set_rate

echo 'Load bitstream'
/bin/cat /tmp/instrument/${CURRENT_INSTRUMENT}.bit > /dev/xdevcfg
/bin/cat /sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done

echo 'Restart tcp-server'
/bin/cp -f /tmp/instrument/kserverd /usr/local/tcp-server/kserverd
/bin/systemctl start tcp-server.service
