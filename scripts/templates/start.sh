#!/bin/bash

echo 'Clock initialization...'
devcfg=/sys/devices/soc0/amba/f8007000.devcfg

set_fclk () {
    local fclk_name=$1
    local fclk_val=$2
    test -d $devcfg/fclk/$fclk_name || echo $fclk_name > $devcfg/fclk_export
    echo 1 > $devcfg/fclk/$fclk_name/enable
    echo $fclk_val > $devcfg/fclk/$fclk_name/set_rate
}

{% for clk in ['fclk0','fclk1','fclk2','fclk3'] -%}
{% if clk in dic['parameters'] -%}
set_fclk {{ clk }} {{ dic['parameters'][clk] }}
{% endif -%}
{% endfor %}

echo 'Load bitstream'
/bin/cat /tmp/instrument/${CURRENT_INSTRUMENT}.bit > /dev/xdevcfg
/bin/cat /sys/bus/platform/drivers/xdevcfg/f8007000.devcfg/prog_done

echo 'Restart tcp-server'
/bin/cp -f /tmp/instrument/kserverd /usr/local/tcp-server/kserverd
/bin/systemctl start tcp-server.service
