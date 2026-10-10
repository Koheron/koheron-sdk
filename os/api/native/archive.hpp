#pragma once
#include "common.hpp"

#include <zip.h>
#include <functional>

namespace koheron::management {
enum class FpgaLoader { any, overlay, xdevcfg };
class InvalidArchive : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class DeploymentError : public std::runtime_error {
public:
    std::string code;
    std::string rollback = "not_needed";
    DeploymentError(std::string value, std::string message) : std::runtime_error(std::move(message)), code(std::move(value)) {}
};
struct ArchiveInfo {
    std::uint64_t extracted_bytes = 0, entries = 0;
    bool bit = false, binary = false, overlay = false, executable = false;
    std::string executable_header;
    Json json() const;
};
class Archive {
    struct Deleter { void operator()(zip_t* value) const noexcept { zip_discard(value); } };
    std::unique_ptr<zip_t, Deleter> archive_;
public:
    explicit Archive(const fs::path& path);
    [[nodiscard]] std::optional<std::string> member(std::string_view name, std::size_t limit = 4 * 1024 * 1024);
    void validate();
    [[nodiscard]] ArchiveInfo inspect(bool verify_crc = true);
    void extract(const fs::path& destination, bool omit_reference_bitstreams = false);
};
void stage_archive(const fs::path& archive, const fs::path& destination, FpgaLoader loader = FpgaLoader::any);
void extract_default(const Settings& settings, std::string_view loader = "auto");
struct Preflight {
    ArchiveInfo info;
    std::string version, code, message;
    std::vector<std::string> warnings;
    Json metadata;
    std::uint64_t available_bytes = 0, required_bytes = 0;
    [[nodiscard]] bool ready() const { return code.empty(); }
    Json json() const;
};
[[nodiscard]] Preflight preflight(const fs::path& archive, const Settings& settings);
using Progress = std::function<void(std::string_view)>;
void install(const fs::path& archive, const Settings& settings, Progress progress = {});
} // namespace koheron::management
