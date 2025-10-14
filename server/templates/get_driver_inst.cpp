#include "server/runtime/services.hpp"
#include "server/runtime/driver_manager.hpp"
#include "server/core/configs/drivers_config.hpp"

namespace rt {

{% for driver in drivers %}
template {{ driver.objects[0]["type"] }}& get_driver<{{ driver.objects[0]["type"] }}>();
{% endfor -%}

} // namespace rt
