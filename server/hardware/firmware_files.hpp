#ifndef KOHERON_FIRMWARE_FILES_HPP
#define KOHERON_FIRMWARE_FILES_HPP

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace hw {

struct FirmwarePreparation {
    std::error_code error;
    std::filesystem::path failed_path;
};

namespace detail {

inline bool enable_live_firmware_path(const std::filesystem::path& live_dir,
                                     const std::filesystem::path& parameter) {
    std::string configured;
    {
        std::ifstream input(parameter);
        if (!std::getline(input, configured)) return false;
    }

    if (!configured.empty()) {
        // Preserve an explicitly configured firmware directory.
        std::error_code ec;
        return std::filesystem::equivalent(configured, live_dir, ec) && !ec;
    }

    const auto path = live_dir.string();
    // firmware_class.path is a 256-byte kernel module parameter, including NUL.
    if (path.size() >= 256) return false;
    std::ofstream output(parameter);
    output << path; // Sysfs parameters do not need a trailing newline.
    output.close();
    return !output.fail();
}

} // namespace detail

inline FirmwarePreparation prepare_firmware_files(
    const std::filesystem::path& live_dir,
    const std::filesystem::path& bitbin,
    const std::filesystem::path& firmware_dir,
    const std::filesystem::path& search_path_parameter) {
    namespace fs = std::filesystem;
    const std::array<fs::path, 2> files{"pl.dtbo", bitbin};

    // Reject missing/empty live payloads before the kernel could fall back to
    // an older file in /lib/firmware.
    for (const auto& name : files) {
        const auto source = live_dir / name;
        std::error_code ec;
        if (!fs::is_regular_file(source, ec)) {
            return {ec ? ec : std::make_error_code(std::errc::invalid_argument), source};
        }
        const auto size = fs::file_size(source, ec);
        if (ec || size == 0) {
            return {ec ? ec : std::make_error_code(std::errc::invalid_argument), source};
        }
    }

    if (detail::enable_live_firmware_path(live_dir, search_path_parameter)) return {};

    // Older kernels and unavailable/customized parameters keep the copy path.
    std::error_code ec;
    fs::create_directories(firmware_dir, ec);
    if (ec) return {ec, firmware_dir};
    for (const auto& name : files) {
        const auto destination = firmware_dir / name;
        fs::copy_file(live_dir / name, destination, fs::copy_options::overwrite_existing, ec);
        if (ec) return {ec, destination};
    }
    return {};
}

} // namespace hw

#endif
