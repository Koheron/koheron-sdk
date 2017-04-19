#!/bin/bash

echo 'Starting instrument'
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
{% if clk in config['parameters'] -%}
set_fclk {{ clk }} {{ config['parameters'][clk] }}
{% endif -%}
{% endfor %}
