namespace eval config {

##########################################################
# Define register offsets
##########################################################
{% for address in config['memory'] if address['registers'] -%}
variable register_{{ address['name'] }}
{% for name in address['registers'] -%}
set register_{{ address['name'] }}({{ loop.index0 }}) {{name}}
{% endfor -%}
variable register_count_{{ address['name'] }} {{ address['register_count'] }}
{% endfor -%}

##########################################################
# Define parameters
##########################################################
{% for key in config['parameters'] -%}
variable {{ key }} {{ config['parameters'][key] }}
{% endfor -%}

##########################################################
# Define offsets and ranges of AXI Slaves
##########################################################
{% for address in config['memory'] -%}
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

