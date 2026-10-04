#include "server/hardware/firmware_files.hpp"

#include <cstdio>

int main(int argc, char** argv) {
    if (argc != 4) return 2;
    const auto result = hw::prepare_firmware_files(argv[1], "test.bit.bin", argv[2], argv[3]);
    if (result.error) {
        std::fprintf(stderr, "%s: %s\n", result.failed_path.c_str(), result.error.message().c_str());
        return 1;
    }
    return 0;
}
