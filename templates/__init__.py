
{% for file in dic['include'] -%}
from {{ file }} import *
{% endfor %}

from {{ dic['driver']['file'] | remove_extension }} import {{ dic['driver']['name'] }} as driver

__all__ = [{{ dic['include'] | quote | join(', ')}}]
