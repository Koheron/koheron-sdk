namespace eval config {

##########################################################
# Define config offsets
##########################################################
{% for offset in dic['config_offsets'] -%}
variable {{ offset }}_offset {{ loop.index0 }}
{% endfor -%}

variable config_size {{ dic['config_offsets'] | length }}

##########################################################
# Define status offsets
##########################################################
set sts_start 10

{% for offset in dic['status_offsets'] -%}
variable {{ offset }}_offset [expr {{ loop.index0 }} + $sts_start]
{% endfor -%}

variable status_size [expr {{ dic['status_offsets'] | length }} + $sts_start]

##########################################################
# Define parameters
##########################################################
{% for key in dic['parameters'] -%}
variable {{ key }} {{ dic['parameters'][key] }}
{% endfor -%}

##########################################################
# Define offsets and ranges of AXI Slaves
##########################################################
{% for address in dic['addresses'] -%}
# {{ address['name'] | upper}}
variable axi_{{ address['name'] }}_offset {{ address['offset'] }}
variable axi_{{ address['name'] }}_range {{ address['range'] }}
{% endfor -%}

} ;# end config namespace

