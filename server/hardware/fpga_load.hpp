#ifndef KOHERON_FPGA_LOAD_HPP
#define KOHERON_FPGA_LOAD_HPP

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include <fcntl.h>
#include <unistd.h>

namespace hw {

// Sysfs/configfs attributes interpret each write as a complete command. Do not
// buffer it, create a missing attribute, or retry a short write as a new command.
inline std::error_code write_fpga_attribute(const std::filesystem::path& path,
                                          std::string_view value) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) return {errno, std::generic_category()};
    ssize_t written;
    do {
        written = ::write(fd, value.data(), value.size());
    } while (written < 0 && errno == EINTR);
    const auto error = written < 0 ? std::error_code(errno, std::generic_category())
        : (static_cast<size_t>(written) != value.size()
           ? std::make_error_code(std::errc::io_error) : std::error_code{});
    const int closed = ::close(fd);
    if (error) return error;
    if (closed < 0) return {errno, std::generic_category()};
    return {};
}

inline bool fpga_attribute_is(const std::filesystem::path& path,
                              std::string_view expected) {
    std::ifstream input(path);
    std::string value;
    return std::getline(input, value) && value == expected;
}

inline bool fpga_overlay_applied(const std::filesystem::path& overlay,
                                 std::string_view firmware_name) {
    // Xilinx configfs path_store() returns a positive count on apply failure.
    // of_overlay_fdt_apply() also sets ov_id=0 on early failure, which makes
    // status_show() incorrectly report "applied". The failed path is cleared.
    return fpga_attribute_is(overlay / "path", firmware_name)
        && fpga_attribute_is(overlay / "status", "applied");
}

} // namespace hw

#endif
