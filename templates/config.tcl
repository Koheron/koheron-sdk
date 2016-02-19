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
set {{ offset }}_offset {{ 8 + loop.index0 }}
{% endfor -%}

##########################################################
# Define parameters
##########################################################
{% for key in dic['parameters'] -%}
set {{ key }} {{ dic['parameters'][key] }}
{% endfor -%}
