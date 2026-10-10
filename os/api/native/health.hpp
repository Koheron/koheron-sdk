#pragma once
#include "common.hpp"
namespace koheron::management {
[[nodiscard]] Json system_health(const Settings& settings, std::uint64_t api_ready, std::uint64_t api_started);
}
