#include "archive.hpp"

#include <array>
#include <iostream>
#include <set>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <algorithm>

namespace koheron::management {
Archive::Archive(const fs::path& path) {
    int error = 0;
    archive_.reset(zip_open(path.c_str(), ZIP_RDONLY | ZIP_CHECKCONS, &error));
    if (!archive_) {
        zip_error_t detail;
        zip_error_init_with_code(&detail, error);
        const std::string message = zip_error_strerror(&detail);
        zip_error_fini(&detail);
        throw InvalidArchive("Invalid instrument archive: " + message);
    }
}

namespace {
struct FileDeleter { void operator()(zip_file_t* value) const noexcept { if (value) zip_fclose(value); } };
using ZipFile = std::unique_ptr<zip_file_t, FileDeleter>;

template<class Consumer>
void read_member(zip_t* archive, zip_uint64_t index, Consumer consume) {
    ZipFile file(zip_fopen_index(archive, index, 0));
    if (!file) throw InvalidArchive(zip_strerror(archive));
    std::array<char, 65536> buffer{};
    for (;;) {
        const auto count = zip_fread(file.get(), buffer.data(), buffer.size());
        if (count < 0) throw InvalidArchive(std::string("Corrupt instrument archive: ") + zip_file_strerror(file.get()));
        if (count == 0) break;
        consume(std::span(buffer.data(), static_cast<std::size_t>(count)));
    }
}

bool payload_exists(const fs::path& destination, FpgaLoader loader) {
    bool bitstream = false, overlay = false, binary = false;
    for (const auto& entry : fs::directory_iterator(destination)) {
        if (!entry.is_regular_file() || entry.file_size() == 0) continue;
        const auto name = entry.path().filename().string();
        bitstream |= name.ends_with(".bit");
        overlay |= name == "pl.dtbo";
        binary |= name.ends_with(".bit.bin");
    }
    if (loader == FpgaLoader::overlay) return overlay && binary;
    if (loader == FpgaLoader::xdevcfg) return bitstream;
    return bitstream || (overlay && binary);
}

FpgaLoader resolve_loader(std::string_view loader) {
    if (loader == "overlay") return FpgaLoader::overlay;
    if (loader == "xdevcfg") return FpgaLoader::xdevcfg;
    if (loader != "auto") throw std::runtime_error("Unknown FPGA loader: " + std::string(loader));
    // Match the server's preference when both loading mechanisms exist.
    if (fs::exists("/dev/xdevcfg")) return FpgaLoader::xdevcfg;
    if (fs::exists("/sys/class/fpga_manager/fpga0/flags")) return FpgaLoader::overlay;
    return FpgaLoader::any; // Offline staging without board devices.
}
}

std::optional<std::string> Archive::member(std::string_view name, std::size_t limit) {
    const auto index = zip_name_locate(archive_.get(), std::string(name).c_str(), 0);
    if (index < 0) return std::nullopt;
    std::string result;
    read_member(archive_.get(), static_cast<zip_uint64_t>(index), [&](std::span<const char> bytes) {
        if (bytes.size() > limit - result.size()) throw InvalidArchive("Archive metadata exceeds size limit");
        result.append(bytes.data(), bytes.size());
    });
    return result;
}

void Archive::validate() {
    const auto count = zip_get_num_entries(archive_.get(), 0);
    if (count < 0 || count > 10000) throw InvalidArchive("Invalid archive entry count");
    std::uint64_t total = 0;
    for (zip_int64_t index = 0; index < count; ++index) {
        read_member(archive_.get(), static_cast<zip_uint64_t>(index), [&](std::span<const char> bytes) {
            total += bytes.size();
            if (total > 256 * 1024 * 1024) throw InvalidArchive("Extracted archive exceeds 256 MiB limit");
        });
    }
}

Json ArchiveInfo::json() const {
    auto value = object(); add(value, "extracted_bytes", integer(extracted_bytes)); add(value, "entries", integer(entries));
    add(value, "reference_bitstream", boolean(bit)); add(value, "fpga_binary", boolean(binary)); add(value, "overlay", boolean(overlay));
    return value;
}
ArchiveInfo Archive::inspect(bool verify_crc) {
    ArchiveInfo info;
    const auto count = zip_get_num_entries(archive_.get(), 0);
    if (count < 0 || count > 10000) throw InvalidArchive("Invalid archive entry count");
    info.entries = count;
    std::set<fs::path> names;
    for (zip_int64_t index = 0; index < count; ++index) {
        zip_stat_t stat{};
        if (zip_stat_index(archive_.get(), index, 0, &stat) < 0 || !(stat.valid & ZIP_STAT_SIZE)) throw InvalidArchive("Missing archive member size");
        const std::string name(stat.name);
        const fs::path path(name); auto canonical = path.lexically_normal();
        if (path.empty() || path.is_absolute() || name.contains('\\') || name.contains('\n') || name.contains('\r') ||
            canonical == "." || canonical.filename() == ".instrument-name") throw InvalidArchive("Unsafe archive member: " + name);
        for (const auto& component : path) if (component == "..") throw InvalidArchive("Unsafe archive member: " + name);
        if (canonical.filename().empty()) canonical = canonical.parent_path();
        if (!names.insert(canonical).second) throw InvalidArchive("Duplicate archive member: " + name);
        zip_uint8_t os = 0; zip_uint32_t attributes = 0;
        if (zip_file_get_external_attributes(archive_.get(), index, 0, &os, &attributes) < 0) throw InvalidArchive("Invalid archive member mode");
        const unsigned mode = os == ZIP_OPSYS_UNIX ? attributes >> 16 : 0, type = mode & S_IFMT;
        if (type != 0 && type != S_IFREG && type != S_IFDIR) throw InvalidArchive("Unsafe archive member type: " + name);
        if (stat.size > 256 * 1024 * 1024 - info.extracted_bytes) throw InvalidArchive("Extracted archive exceeds 256 MiB limit");
        info.extracted_bytes += stat.size;
        const bool regular = !name.ends_with('/') && type != S_IFDIR && stat.size != 0;
        info.bit |= regular && path.parent_path().empty() && name.ends_with(".bit");
        info.binary |= regular && path.parent_path().empty() && name.ends_with(".bit.bin");
        info.overlay |= regular && name == "pl.dtbo";
        info.executable |= regular && name == "serverd" && (mode & 0111);
        if (verify_crc || name == "serverd") {
            std::uint64_t actual = 0;
            read_member(archive_.get(), index, [&](std::span<const char> bytes) {
                if (bytes.size() > stat.size - actual) throw InvalidArchive("Archive member size mismatch");
                actual += bytes.size();
                if (name == "serverd" && info.executable_header.size() < 64)
                    info.executable_header.append(bytes.data(), std::min(bytes.size(), 64 - info.executable_header.size()));
            });
            if (actual != stat.size) throw InvalidArchive("Archive member size mismatch");
        }
    }
    return info;
}

Json Preflight::json() const {
    auto value = object(), notes = array();
    for (const auto& warning : warnings) append(notes, text(warning));
    add(value, "ready", boolean(ready())); add(value, "code", text(code)); add(value, "message", text(message));
    add(value, "version", text(version)); add(value, "archive", info.json()); add(value, "warnings", std::move(notes));
    add(value, "available_bytes", integer(available_bytes)); add(value, "required_bytes", integer(required_bytes));
    add(value, "metadata", metadata ? parse(encode(metadata)) : Json{});
    return value;
}
Preflight preflight(const fs::path& filename, const Settings& settings) {
    Preflight result;
    const auto fail = [&](std::string code, std::string message) { result.code = std::move(code); result.message = std::move(message); };
    try {
        if (!fs::is_regular_file(fs::symlink_status(filename))) throw InvalidArchive("Instrument archive is not a regular file");
        Archive archive(filename); result.info = archive.inspect();
        const auto version = archive.member("version", 4096);
        if (!result.info.executable) throw InvalidArchive("Instrument serverd is not executable");
        if (!version || trim(*version).empty() || utf8(*version) != *version) throw InvalidArchive("Instrument version is missing or invalid");
        result.version = trim(*version);
        const auto loader = resolve_loader("auto");
        if ((loader == FpgaLoader::overlay && !(result.info.overlay && result.info.binary)) ||
            (loader == FpgaLoader::xdevcfg && !result.info.bit) ||
            (loader == FpgaLoader::any && !(result.info.bit || (result.info.overlay && result.info.binary))))
            throw InvalidArchive("Instrument archive has no FPGA payload for the selected loader");
        utsname machine{}; if (::uname(&machine) < 0) system_error("machine architecture");
        const std::string_view hardware(machine.machine);
        const std::string architecture = hardware == "aarch64" ? "arm64" : hardware.starts_with("arm") ? "armhf" : hardware == "x86_64" ? "amd64" : std::string(hardware);
        const auto& header = result.info.executable_header;
        if (header.size() >= 20 && header.substr(0, 4) == std::string("\177ELF", 4)) {
            const unsigned elf_machine = static_cast<unsigned char>(header[18]) | (static_cast<unsigned char>(header[19]) << 8);
            const unsigned expected = architecture == "armhf" ? 40 : architecture == "arm64" ? 183 : architecture == "amd64" ? 62 : 0;
            const unsigned elf_class = architecture == "armhf" ? 1 : 2;
            if (expected && (elf_machine != expected || header[5] != 1 || static_cast<unsigned char>(header[4]) != elf_class))
                fail("architecture_mismatch", "Instrument executable does not match this board's CPU architecture");
        } else result.warnings.push_back("Executable architecture could not be verified");
        const auto metadata = archive.member("instrument.json", 16384);
        if (metadata) {
            try { result.metadata = parse(*metadata); }
            catch (const std::exception&) { throw InvalidArchive("Invalid instrument compatibility JSON"); }
            json_object *format = nullptr, *minimum = nullptr;
            if (!result.metadata || !json_object_is_type(result.metadata.get(), json_type_object) ||
                !json_object_object_get_ex(result.metadata.get(), "format", &format) || !json_object_is_type(format, json_type_int) || json_object_get_int(format) != 1 ||
                !json_object_object_get_ex(result.metadata.get(), "min_runtime_api", &minimum) || !json_object_is_type(minimum, json_type_int) ||
                field(result.metadata.get(), "board").empty() || field(result.metadata.get(), "architecture").empty() || field(result.metadata.get(), "sdk_version").empty())
                throw InvalidArchive("Invalid instrument compatibility metadata");
            const auto board = kv_file(settings.manifest);
            if (field(result.metadata.get(), "architecture") != architecture)
                fail("architecture_mismatch", "Instrument package targets a different CPU architecture");
            else if (board && !field(board.get(), "board").empty() && field(board.get(), "board") != field(result.metadata.get(), "board"))
                fail("board_mismatch", "Instrument package targets a different board");
            else if (json_object_get_int(minimum) > 1 || json_object_get_int(minimum) < 1 || !field(result.metadata.get(), "sdk_version").starts_with("1."))
                fail("runtime_mismatch", "Instrument package requires an incompatible runtime");
            if (!board || field(board.get(), "board").empty()) result.warnings.push_back("Board identity is unavailable; board compatibility is unverified");
            if (header.size() < 20 || header.substr(0, 4) != std::string("\177ELF", 4))
                throw InvalidArchive("Instrument with compatibility metadata must contain an ELF executable");
        } else result.warnings.push_back("Older package: board compatibility is unverified");
        auto parent = fs::absolute(settings.live).parent_path();
        while (!parent.empty() && !fs::exists(parent)) parent = parent.parent_path();
        struct statvfs space{};
        if (parent.empty() || ::statvfs(parent.c_str(), &space) < 0) system_error("staging free space");
        result.available_bytes = static_cast<std::uint64_t>(space.f_bavail) * space.f_frsize;
        result.required_bytes = result.info.extracted_bytes + result.info.entries * std::max<std::uint64_t>(4096, space.f_frsize) + 4 * 1024 * 1024;
        if (result.ready() && result.available_bytes < result.required_bytes) fail("insufficient_space", "Not enough free space to stage this instrument while keeping the current installation");
    } catch (const InvalidArchive& error) { fail("invalid_archive", error.what()); }
    catch (const std::exception& error) { fail("preflight_failed", error.what()); }
    return result;
}

void Archive::extract(const fs::path& destination, bool omit_reference_bitstreams) {
    std::set<fs::path> names;
    std::uint64_t total = 0;
    const auto count = zip_get_num_entries(archive_.get(), 0);
    if (count < 0 || count > 10000) throw InvalidArchive("Invalid archive entry count");
    for (zip_int64_t index = 0; index < count; ++index) {
        const auto id = static_cast<zip_uint64_t>(index);
        const char* raw_name = zip_get_name(archive_.get(), id, 0);
        if (!raw_name) throw InvalidArchive(zip_strerror(archive_.get()));
        const std::string name(raw_name);
        fs::path path(name);
        const auto normalized = path.lexically_normal();
        if (path.empty() || path.is_absolute() || name.contains('\\') || name.contains('\n') || name.contains('\r') ||
            normalized == "." || normalized.filename() == ".instrument-name")
            throw InvalidArchive("Unsafe archive member: " + name);
        for (const auto& component : path) if (component == "..") throw InvalidArchive("Unsafe archive member: " + name);
        // A trailing directory slash must not evade duplicate detection.
        auto canonical = normalized;
        if (canonical.filename().empty()) canonical = canonical.parent_path();
        if (!names.insert(canonical).second) throw InvalidArchive("Duplicate archive member: " + name);
        zip_uint8_t operating_system = 0;
        zip_uint32_t attributes = 0;
        if (zip_file_get_external_attributes(archive_.get(), id, 0, &operating_system, &attributes) < 0)
            throw InvalidArchive(zip_strerror(archive_.get()));
        const unsigned mode = operating_system == ZIP_OPSYS_UNIX ? attributes >> 16 : 0;
        const unsigned type = mode & S_IFMT;
        if (type != 0 && type != S_IFREG && type != S_IFDIR) throw InvalidArchive("Unsafe archive member type: " + name);
        const auto target = destination / canonical;
        if (name.ends_with('/') || type == S_IFDIR) {
            fs::create_directories(target);
            continue;
        }
        Fd output;
        if (!(omit_reference_bitstreams && name.ends_with(".bit"))) {
            fs::create_directories(target.parent_path());
            output = Fd(::open(target.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0644 | (mode & 0111)));
            if (output.get() < 0) system_error("extract " + name);
        }
        read_member(archive_.get(), id, [&](std::span<const char> bytes) {
            total += bytes.size();
            if (total > 256 * 1024 * 1024) throw InvalidArchive("Extracted archive exceeds 256 MiB limit");
            // Omitted reference bitstreams still count toward the size bound
            // and are read completely to check their CRC.
            if (output.get() >= 0) write_all(output.get(), bytes);
        });
    }
}

void stage_archive(const fs::path& filename, const fs::path& destination, FpgaLoader loader) {
    Archive archive(filename);
    archive.extract(destination, loader == FpgaLoader::overlay);
    const auto version = read_file(destination / "version");
    if (!fs::is_regular_file(destination / "serverd") || ::access((destination / "serverd").c_str(), X_OK) != 0)
        throw InvalidArchive("Instrument serverd is not executable");
    if (!version || trim(*version).empty()) throw InvalidArchive("Instrument version is empty");
    if (utf8(*version) != *version) throw InvalidArchive("Instrument version is not valid UTF-8");
    if (!payload_exists(destination, loader)) throw InvalidArchive("Instrument archive has no FPGA payload for the selected loader");
    write_file(destination / ".instrument-name", filename.stem().string() + "\n");
}

void extract_default(const Settings& settings, std::string_view requested_loader) {
    const auto loader = resolve_loader(requested_loader);
    const auto preference = read_file(settings.instruments / "default", 4096);
    if (!preference) throw InvalidArchive("Default instrument file is missing");
    const auto name = trim(*preference);
    if (name.size() <= 4 || !name.ends_with(".zip") || safe_filename(name) != name)
        throw InvalidArchive("Invalid default instrument filename");
    const auto filename = settings.instruments / name;
    if (!fs::is_regular_file(fs::symlink_status(filename)))
        throw InvalidArchive("Default instrument archive is missing or not a regular file");
    auto live = fs::absolute(settings.live).lexically_normal();
    if (live != live.root_path() && live.filename().empty()) live = live.parent_path();
    if (live.filename().empty() || live == live.root_path())
        throw std::runtime_error("Invalid live instrument directory");
    const auto status = fs::symlink_status(live);
    if (fs::exists(status) && !fs::is_directory(status))
        throw std::runtime_error("Live instrument path is not a directory");
    const auto relative_archive = fs::weakly_canonical(filename).lexically_relative(fs::weakly_canonical(live));
    if (!relative_archive.empty() && *relative_archive.begin() != "..")
        throw std::runtime_error("Live instrument directory contains the source archive");
    fs::create_directories(live.parent_path());
    const auto transaction = temporary_directory(live.parent_path(), ".instrument-");
    const auto staged = transaction / "next", backup = transaction / "previous";
    try {
        fs::create_directory(staged);
        stage_archive(filename, staged, loader);
        if (fs::exists(live)) fs::rename(live, backup);
        try { fs::rename(staged, live); }
        catch (...) {
            if (fs::exists(backup)) fs::rename(backup, live);
            throw;
        }
        fs::remove_all(backup);
    } catch (...) {
        if (fs::exists(backup)) std::cerr << "Recovery failed; previous files retained at " << backup << '\n';
        else fs::remove_all(transaction);
        throw;
    }
    fs::remove_all(transaction);
}

void install(const fs::path& filename, const Settings& settings, Progress progress) {
    const auto report = [&](std::string_view phase) { if (progress) progress(phase); };
    report("validating");
    const auto check = preflight(filename, settings);
    if (!check.ready()) throw DeploymentError(check.code, check.message);
    const auto transaction = temporary_directory(settings.live.parent_path(), ".instrument-");
    const auto staged = transaction / "next", backup = transaction / "previous";
    try {
        fs::create_directory(staged);
        report("extracting");
        stage_archive(filename, staged);
        const bool was_active = unit_is_active(settings);
        report("stopping");
        try { service_action(settings, "stop", settings.unit); }
        catch (const std::exception& error) { throw DeploymentError("stop_failed", error.what()); }
        bool activated = false;
        try {
            if (fs::exists(settings.live)) fs::rename(settings.live, backup);
            fs::rename(staged, settings.live);
            activated = true;
            report("starting");
            service_action(settings, "start", settings.unit);
        } catch (...) {
            const auto activation_error = std::current_exception();
            report("rolling_back");
            try {
                if (activated) {
                    service_action(settings, "stop", settings.unit);
                    fs::remove_all(settings.live);
                }
                if (fs::exists(backup)) fs::rename(backup, settings.live);
                if (was_active) service_action(settings, "start", settings.unit);
            } catch (const std::exception& error) {
                DeploymentError failure("rollback_failed", error.what()); failure.rollback = "failed"; throw failure;
            }
            std::string reason;
            try { std::rethrow_exception(activation_error); } catch (const std::exception& error) { reason = error.what(); }
            DeploymentError failure(activated ? "start_failed" : "activation_failed", reason);
            failure.rollback = "restored"; throw failure;
        }
        fs::remove_all(backup);
        try { service_action(settings, "start", settings.led_unit); }
        catch (const std::exception& error) { std::cerr << "Instrument loaded; LED initialization failed: " << error.what() << '\n'; }
    } catch (...) {
        if (fs::exists(backup)) std::cerr << "Recovery failed; previous files retained at " << backup << '\n';
        else fs::remove_all(transaction);
        throw;
    }
    fs::remove_all(transaction);
}
} // namespace koheron::management
