namespace eval config {

##########################################################
# Define config offsets
##########################################################
variable cfg_register
{% for name in dic['config_registers'] -%}
set cfg_register({{ loop.index0 }}) {{name}}
{% endfor -%}

variable config_size {{ dic['config_registers'] | length }}

##########################################################
# Define status offsets
##########################################################
set sts_start 10
variable sts_register
{% for name in dic['status_registers'] -%}
set sts_register([expr {{ loop.index0 }} + $sts_start]) {{name}}
{% endfor -%}

variable status_size [expr {{ dic['status_registers'] | length }} + $sts_start]

##########################################################
# Define parameters
##########################################################
{% for key in dic['parameters'] -%}
variable {{ key }} {{ dic['parameters'][key] }}
{% endfor -%}

##########################################################
# Define offsets and ranges of AXI Slaves
##########################################################
{% for address in dic['memory'] -%}
{% if address['n_blocks'] == 1 %}
variable memory_{{ address['name'] }}_offset {{ address['offset'] }}
variable memory_{{ address['name'] }}_range {{ address['range'] }}
{% else %}
{% for i in range(address['n_blocks']) -%}
variable memory_{{ address['name'] }}{{ i }}_offset 0x[format %x [expr {{ address['offset'] }} + {{ i }} * {{ address['range']|replace_KMG }}]]
variable memory_{{ address['name'] }}{{ i }}_range {{ address['range'] }}
{% endfor %}
{% endif %}
{% endfor -%}

} ;# end config namespace

