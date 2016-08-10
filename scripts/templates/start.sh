#!/bin/bash

echo 'Clock initialization...'
/bin/bash /tmp/instrument/start.sh

devcfg=/sys/devices/soc0/amba/f8007000.devcfg

clk0 = {{ dic['parameters']['clk0'] }}
clk1 = {{ dic['parameters']['clk1'] }}

function set_fclk () {
    fclk_name = $1
    fclk_val = $2
    test -d $devcfg/fclk/$fclk_name || echo $fclk_name > $devcfg/fclk_export
    echo 1 > $devcfg/fclk/$fclk_name/enable
    echo $fclk_val > $devcfg/fclk/$fclk_name/set_rate
}

set_fclk fclk0 $clk0
set_fclk fclk1 $clk1

echo 'Load bitstream'
/bin/cat /tmp/instrument/${CURRENT_INSTRUMENT}.bit > /dev/xdevcfg
/bin/cat /sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done

echo 'Restart tcp-server'
/bin/cp -f /tmp/instrument/kserverd /usr/local/tcp-server/kserverd
/bin/systemctl start tcp-server.service
