/// Hardware addresses
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_ADDRESSES_HPP__
#define __DRIVERS_CORE_ADDRESSES_HPP__

// -- Base addresses
{% for addr in dic['addresses'] -%}
#define {{ addr['name']|upper }}_ADDR {{ addr['offset'] }}
#define {{ addr['name']|upper }}_RANGE {{ addr['range']|replace('K','*1024') }}
{% endfor %}

// -- Config offsets
{% for offset in dic['config_offsets'] -%}
#define {{ offset|upper }}_OFF {{ 4 * loop.index0 }}
{% endfor %}
// -- Status offsets
{% for offset in dic['status_offsets'] -%}
#define {{ offset|upper }}_OFF {{ 4 * (8 + loop.index0) }}
{% endfor %}
#endif // __DRIVERS_CORE_ADDRESSES_HPP__

#define BITSTREAM_ID_OFF 0
