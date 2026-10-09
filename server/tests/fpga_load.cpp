#include "server/hardware/fpga_load.hpp"

#include <cstdio>
#include <string_view>

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    if (std::string_view(argv[1]) == "write") {
        const auto error = hw::write_fpga_attribute(argv[2], argv[3]);
        if (error) {
            std::fprintf(stderr, "%s\n", error.message().c_str());
            return 1;
        }
        return 0;
    }
    if (std::string_view(argv[1]) == "overlay") {
        return hw::fpga_overlay_applied(argv[2], argv[3]) ? 0 : 1;
    }
    return hw::fpga_attribute_is(argv[2], argv[3]) ? 0 : 1;
}
