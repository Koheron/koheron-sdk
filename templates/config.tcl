##########################################################
# Define config offsets
##########################################################
{% for offset in dic['config_offsets'] -%}
set {{ offset }}_offset {{ loop.index0 }}
{% endfor -%}

##########################################################
# Define status offsets
##########################################################
{% for offset in dic['status_offsets'] -%}
set {{ offset }}_offset {{ 10 + loop.index0 }}
{% endfor -%}

##########################################################
# Define parameters
##########################################################
{% for key in dic['parameters'] -%}
set {{ key }} {{ dic['parameters'][key] }}
{% endfor -%}

##########################################################
# Define offsets and ranges of AXI Slaves
##########################################################
{% for address in dic['addresses'] -%}
# {{ address['name'] | upper}}
set axi_{{ address['name'] }}_offset {{ address['offset'] }}
set axi_{{ address['name'] }}_range {{ address['range'] }}
{% endfor -%}

