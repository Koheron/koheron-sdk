# This file is auto-generated : do no edit !

{% for addr in dic['addresses'] -%}
# {{ addr['name']| capitalize }}:
{{ addr['name']|upper }}_ADDR  = int('{{ addr['offset'] }}', 0)
{{ addr['name']|upper }}_RANGE = {{ addr['range']|replace('K','*1024') }}

{% endfor %}

# Config offsets
{% for offset in dic['config_offsets'] -%}
{{ offset|upper }}_OFF = {{ 4 * loop.index0 }}
{% endfor %}

# Status offsets
{% for offset in dic['status_offsets'] -%}
{{ offset|upper }}_OFF = {{ 4 * (10 + loop.index0) }}
{% endfor %}

BITSTREAM_ID_OFF = 0
DNA_OFF = 4 * 8
