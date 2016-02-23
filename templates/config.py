import os
import sys
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../..')))
from remote_control import ZynqHTTP #, ZynqSSH

from koheron_tcp_client import KClient, DevMem

host = os.getenv('HOST','{{ dic['host'] }}')

client = KClient(host)
dvm = DevMem(client)

# ---------------------
# Load instrument
# ---------------------

#ssh = ZynqSSH(host, 'changeme')
http = ZynqHTTP(host)

zipfilename = '{{ dic['project'] + '-' + version }}.zip'
zippath = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../tmp', zipfilename))

# ---------------------
# Memory maps
# ---------------------

{% for addr in dic['addresses'] -%}
# {{ addr['name']| capitalize }}:
{{ addr['name']|upper }}_ADDR  = int('{{ addr['offset'] }}', 0)
{{ addr['name']|upper }}_RANGE = {{ addr['range']|replace('K','*1024') }}
{{ addr['name']|upper }} = dvm.add_memory_map({{ addr['name']|upper }}_ADDR, {{ addr['name']|upper }}_RANGE)

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
