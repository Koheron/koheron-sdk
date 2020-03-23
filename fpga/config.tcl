namespace eval config {

##########################################################
# Define control offsets
##########################################################
variable ctl_register
{% for name in config['control_registers'] -%}
set ctl_register({{ loop.index0 }}) {{name}}
{% endfor -%}

variable control_size {{ config['control_registers'] | length }}

##########################################################
# Define ps control offsets
##########################################################
variable ps_ctl_register
{% for name in config['ps_control_registers'] -%}
set ps_ctl_register({{ loop.index0 }}) {{name}}
{% endfor -%}

variable ps_control_size {{ config['ps_control_registers'] | length }}

##########################################################
# Define status offsets
##########################################################
set sts_start 2
variable sts_register
{% for name in config['status_registers'] -%}
set sts_register([expr {{ loop.index0 }} + $sts_start]) {{name}}
{% endfor -%}

variable status_size [expr {{ config['status_registers'] | length }} + $sts_start]

##########################################################
# Define ps status offsets
##########################################################
variable ps_sts_register
{% for name in config['ps_status_registers'] -%}
set ps_sts_register({{ loop.index0 }}) {{name}}
{% endfor -%}

variable ps_status_size {{ config['ps_status_registers'] | length }}

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

