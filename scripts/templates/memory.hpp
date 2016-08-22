/// (c) Koheron

#ifndef __DRIVERS_MEMORY_HPP__
#define __DRIVERS_MEMORY_HPP__

#include <array>
#include <tuple>
#include <cstdint>

extern "C" {
  #include <sys/mman.h> // PROT_READ, PROT_WRITE
}

namespace mem {
{% for addr in dic['memory'] -%}
constexpr uint32_t {{ addr['name'] }} = {{ loop.index0 }};
constexpr uintptr_t {{ addr['name'] }}_addr = {{ addr['offset'] }};
constexpr uint32_t {{ addr['name'] }}_range = {{ addr['range'] | replace_KMG }};
constexpr uint32_t {{ addr['name'] }}_nblocks = {{ addr['n_blocks'] }};
{% endfor %}

constexpr uint32_t count = {{ dic['memory']|length }};

constexpr std::array<std::tuple<uintptr_t, uint32_t, int, uint32_t>, count> memory_array = {{ '{{' }}
    {% for addr in dic['memory'] -%}
        {% if not loop.last -%}
            std::make_tuple({{ addr['name'] }}_addr, {{ addr['name'] }}_range, {{ addr['prot_flag'] }}, {{ addr['name'] }}_nblocks),
        {% else -%}
            std::make_tuple({{ addr['name'] }}_addr, {{ addr['name'] }}_range, {{ addr['prot_flag'] }}, {{ addr['name'] }}_nblocks)
        {% endif -%}
    {% endfor -%}
{{ '}};' }}

} // namespace mem

namespace reg {
// -- Config offsets
{% for offset in dic['config_registers'] -%}
constexpr uint32_t {{ offset }} = {{ 4 * loop.index0 }};
static_assert({{ offset }} < mem::config_range, "Invalid config register offset {{ offset }}");
{% endfor %}
// -- Status offsets
{% for offset in dic['status_registers'] -%}
constexpr uint32_t {{ offset }} = {{ 4 * (10 + loop.index0) }};
static_assert({{ offset }} < mem::status_range, "Invalid status register offset {{ offset }}");
{% endfor %}

constexpr uint32_t bitstream_id = 0;
} // namespace reg

namespace prm {
{% for key in dic['parameters'] -%}
constexpr uint32_t {{ key }} = {{ dic['parameters'][key] }};
{% endfor %}

constexpr uint32_t bitstream_id_size = 8;

constexpr uint32_t dna = 4 * 8;
} // namespace prm

// -- JSONified config
constexpr auto CFG_JSON = "{{ dic['json'] }}";

#endif // __DRIVERS_MEMORY_HPP__
