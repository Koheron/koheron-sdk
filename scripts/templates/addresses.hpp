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
#define {{ offset|upper }}_OFF {{ 4 * (10 + loop.index0) }}
{% endfor %}

// -- Parameters
{% for key in dic['parameters'] -%}
#define {{ key|upper }}_PARAM {{ dic['parameters'][key] }}
{% endfor -%}


#define BITSTREAM_ID_OFF 0
#define BITSTREAM_ID_SIZE 8

#define DNA_OFF 8*4

#endif // __DRIVERS_CORE_ADDRESSES_HPP__
