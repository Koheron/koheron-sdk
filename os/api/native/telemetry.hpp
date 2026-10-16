#pragma once
#include "common.hpp"
namespace koheron::management {
// Consume a bounded snapshot produced independently of instrument lifecycle.
[[nodiscard]] Json board_telemetry(const fs::path& path);
}
