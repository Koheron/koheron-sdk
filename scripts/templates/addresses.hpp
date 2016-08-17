/// Hardware addresses
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_ADDRESSES_HPP__
#define __DRIVERS_CORE_ADDRESSES_HPP__

#include <array>
#include <tuple>
#include <cstdint>

extern "C" {
  #include <sys/mman.h> // PROT_READ, PROT_WRITE
}

// -- Base addresses
{% for addr in dic['addresses'] -%}
constexpr uint32_t {{ addr['name']|upper }}_MEM = {{ loop.index0 }};
constexpr uintptr_t {{ addr['name']|upper }}_ADDR = {{ addr['offset'] }};
constexpr uint32_t {{ addr['name']|upper }}_RANGE = {{ addr['range'] | replace_KMG }};
constexpr uint32_t {{ addr['name']|upper }}_NBLOCKS = {{ addr['n_blocks'] }};
{% endfor %}

constexpr uint32_t NUM_ADDRESSES = {{ dic['addresses']|length }};

constexpr std::array<std::tuple<uintptr_t, uint32_t, int, uint32_t>, NUM_ADDRESSES> address_array = {{ '{{' }}
    {% for addr in dic['addresses'] -%}
        {% if not loop.last -%}
            std::make_tuple({{ addr['name']|upper }}_ADDR, {{ addr['name']|upper }}_RANGE, {{ addr['prot_flag']|upper }}, {{ addr['name']|upper }}_NBLOCKS),
        {% else -%}
            std::make_tuple({{ addr['name']|upper }}_ADDR, {{ addr['name']|upper }}_RANGE, {{ addr['prot_flag']|upper }}, {{ addr['name']|upper }}_NBLOCKS)
        {% endif -%}
    {% endfor -%}
{{ '}};' }}

// -- Config offsets
{% for offset in dic['config_registers'] -%}
constexpr uint32_t {{ offset|upper }}_OFF = {{ 4 * loop.index0 }};
static_assert({{ offset|upper }}_OFF < CONFIG_RANGE, "Invalid config register offset {{ offset }}");
{% endfor %}
// -- Status offsets
{% for offset in dic['status_registers'] -%}
constexpr uint32_t {{ offset|upper }}_OFF = {{ 4 * (10 + loop.index0) }};
static_assert({{ offset|upper }}_OFF < STATUS_RANGE, "Invalid status register offset {{ offset }}");
{% endfor %}

// -- Parameters
{% for key in dic['parameters'] -%}
constexpr uint32_t {{ key|upper }}_PARAM = {{ dic['parameters'][key] }};
{% endfor %}

constexpr uint32_t BITSTREAM_ID_OFF = 0;
constexpr uint32_t BITSTREAM_ID_SIZE = 8;

constexpr uint32_t DNA_OFF = 4 * 8;

// -- JSONified config
#define CFG_JSON "{{ dic['json'] }}"

#endif // __DRIVERS_CORE_ADDRESSES_HPP__
