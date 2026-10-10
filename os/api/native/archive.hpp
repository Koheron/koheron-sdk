#pragma once
#include "common.hpp"

#include <zip.h>

namespace koheron::management {
class InvalidArchive : public std::runtime_error { public: using std::runtime_error::runtime_error; };
class Archive {
    struct Deleter { void operator()(zip_t* value) const noexcept { zip_discard(value); } };
    std::unique_ptr<zip_t, Deleter> archive_;
public:
    explicit Archive(const fs::path& path);
    [[nodiscard]] std::optional<std::string> member(std::string_view name, std::size_t limit = 4 * 1024 * 1024);
    void validate();
    void extract(const fs::path& destination);
};
void stage_archive(const fs::path& archive, const fs::path& destination);
void install(const fs::path& archive, const Settings& settings);
} // namespace koheron::management
