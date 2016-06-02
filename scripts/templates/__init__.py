
{% for file in dic['include'] -%}
from {{ file }} import *
{% endfor %}

__all__ = [{{ dic['include'] | quote | join(', ')}}]
