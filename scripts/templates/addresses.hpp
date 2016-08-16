/// Hardware addresses
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_ADDRESSES_HPP__
#define __DRIVERS_CORE_ADDRESSES_HPP__

#include <tuple>

// -- Base addresses
{% for addr in dic['addresses'] -%}
constexpr uintptr_t {{ addr['name']|upper }}_ADDR = {{ addr['offset'] }};
constexpr uint32_t {{ addr['name']|upper }}_RANGE = {{ addr['range']|replace('K','*1024')|replace('M','*1024*1024')|replace('G','*1024*1024*1024') }};
constexpr uint32_t {{ addr['name']|upper }}_ID = {{ loop.index0 }};
{% endfor %}

#define NUM_ADDRESSES {{ dic['addresses']|length }}

constexpr std::array<std::tuple<uintptr_t, uint32_t, int>, NUM_ADDRESSES> address_array = {{ '{{' }}
    {% for addr in dic['addresses'] -%}
        {% if not loop.last -%}
            std::make_tuple({{ addr['name']|upper }}_ADDR, {{ addr['name']|upper }}_RANGE, {{ addr['prot_flag']|upper }}),
        {% else -%}
            std::make_tuple({{ addr['name']|upper }}_ADDR, {{ addr['name']|upper }}_RANGE, {{ addr['prot_flag']|upper }})
        {% endif -%}
    {% endfor -%}
{{ '}};' }}

// -- Config offsets
{% for offset in dic['config_registers'] -%}
#define {{ offset|upper }}_OFF {{ 4 * loop.index0 }}
{% endfor %}
// -- Status offsets
{% for offset in dic['status_registers'] -%}
#define {{ offset|upper }}_OFF {{ 4 * (10 + loop.index0) }}
{% endfor %}

// -- Parameters
{% for key in dic['parameters'] -%}
#define {{ key|upper }}_PARAM {{ dic['parameters'][key] }}
{% endfor %}

#define BITSTREAM_ID_OFF 0
#define BITSTREAM_ID_SIZE 8

#define DNA_OFF 4 * 8

// -- JSONify config
#define CFG_JSON "{{ dic['json'] }}"

#endif // __DRIVERS_CORE_ADDRESSES_HPP__
