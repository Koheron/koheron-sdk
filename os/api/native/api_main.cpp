#include "archive.hpp"
#include "journal.hpp"
#include "health.hpp"
#include "events.hpp"

#include <microhttpd.h>
#include <systemd/sd-daemon.h>

#include <algorithm>
#include <arpa/inet.h>
#include <array>
#include <atomic>
#include <charconv>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <shared_mutex>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

using namespace koheron::management;
namespace {
constexpr std::size_t max_upload = 20 * 1024 * 1024;
struct Reply {
    unsigned status = 200;
    std::string body;
    std::string type = "text/html; charset=utf-8";
    std::string disposition;
};
Reply json_reply(Json value, unsigned status = 200) { return {status, encode(value) + '\n', "application/json", {}}; }
Reply error_reply(std::string_view error, unsigned status) {
    auto value = object(); add(value, "error", text(error)); return json_reply(std::move(value), status);
}
Reply failure(std::string_view code, std::string_view message, unsigned status = 400, std::string_view rollback = "not_needed") {
    auto value = object(); add(value, "code", text(code)); add(value, "error", text(message)); add(value, "rollback", text(rollback));
    return json_reply(std::move(value), status);
}
struct Operation {
    std::string action, instrument, phase = "idle", code, message, rollback = "not_needed";
    std::uint64_t started_us = 0, finished_us = 0, revision = 0;
    Json json() const {
        auto result = object();
        add(result, "action", text(action)); add(result, "instrument", text(instrument)); add(result, "phase", text(phase));
        add(result, "code", text(code)); add(result, "message", text(message)); add(result, "rollback", text(rollback));
        add(result, "started_us", integer(started_us)); add(result, "finished_us", integer(finished_us)); add(result, "revision", integer(revision));
        add(result, "busy", boolean(started_us != 0 && finished_us == 0)); return result;
    }
};
struct Instrument {
    std::string name, version;
    bool is_default = false;
    Json json() const {
        auto value = object(); add(value, "name", text(name)); add(value, "version", text(version)); add(value, "is_default", boolean(is_default)); return value;
    }
};
struct PendingActivation {
    std::atomic<bool>& flag;
    explicit PendingActivation(std::atomic<bool>& value) : flag(value) { flag.store(true, std::memory_order_release); }
    ~PendingActivation() { flag.store(false, std::memory_order_release); }
    PendingActivation(const PendingActivation&) = delete;
    PendingActivation& operator=(const PendingActivation&) = delete;
};
class App {
    std::mutex state_mutex_;
    std::mutex mutation_mutex_;
    mutable std::shared_mutex live_mutex_;
    std::atomic<bool> activation_pending_{false};
    std::vector<Instrument> inventory_;
    std::optional<std::string> log_cursor_, invocation_;
    std::optional<std::uint64_t> log_timestamp_;
    Operation operation_;
    std::mutex health_mutex_, upload_mutex_;
    std::string cached_health_;
    std::uint64_t health_sample_ = 0;
    std::atomic<std::uint64_t> api_ready_{0};
    const std::uint64_t api_started_ = monotonic_us();
    unsigned uploads_ = 0;

    void phase(std::string_view value) {
        { std::lock_guard lock(state_mutex_); operation_.phase = value; ++operation_.revision; }
        if (events) events->wake();
    }
    void begin(std::string_view action, std::string_view name) {
        { std::lock_guard lock(state_mutex_);
            const auto revision = operation_.revision + 1;
            operation_ = Operation{}; operation_.revision = revision; operation_.action = action;
            operation_.instrument = name; operation_.phase = "validating"; operation_.started_us = realtime_us(); }
        if (events) events->wake();
    }
    void finish(std::string_view code = {}, std::string_view message = {}, std::string_view rollback = "not_needed") {
        { std::lock_guard lock(state_mutex_);
            operation_.phase = code.empty() ? "succeeded" : "failed"; operation_.code = code; operation_.message = message;
            operation_.rollback = rollback; operation_.finished_us = realtime_us(); ++operation_.revision; }
        if (events) events->wake();
    }
    Json operation() { std::lock_guard lock(state_mutex_); return operation_.json(); }

    bool is_default(const fs::path& archive) const {
        const auto preference = read_file(settings.instruments / "default");
        return preference && archive == settings.instruments / trim(*preference);
    }
    Instrument instrument(const fs::path& archive) const {
        std::string version = "0.0.0";
        try { const auto data = Archive(archive).member("version", 4096); if (data && !trim(*data).empty()) version = trim(*data); }
        catch (const InvalidArchive&) {}
        return {archive.stem().string(), std::move(version), is_default(archive)};
    }
    Json identity() const {
        const auto name_file = read_file(settings.live / ".instrument-name", 4096), version_file = read_file(settings.live / "version", 4096);
        if (!name_file || !version_file || utf8(*name_file) != *name_file || utf8(*version_file) != *version_file) return {};
        const auto name = trim(*name_file);
        if (name.empty() || safe_filename(name + ".zip") != name + ".zip") return {};
        return Instrument{name, trim(*version_file), is_default(settings.instruments / (name + ".zip"))}.json();
    }
    Json current() const {
        std::shared_lock lock(live_mutex_, std::try_to_lock);
        if (!lock.owns_lock() || activation_pending_.load(std::memory_order_acquire)) return {};
        try { return identity(); } catch (...) { return {}; }
    }
    Json live() const {
        // shared_mutex can prefer readers indefinitely. Announce a waiting
        // activation so new polling requests let existing snapshots drain.
        if (activation_pending_.load(std::memory_order_acquire)) return {};
        std::shared_lock lock(live_mutex_, std::try_to_lock);
        if (!lock.owns_lock() || activation_pending_.load(std::memory_order_acquire)) return {};
        try {
            if (!unit_is_active(settings)) return {};
            return identity();
        } catch (const std::exception&) { return {}; }
    }
    Reply run(std::string_view requested, bool modern = false) {
        std::unique_lock transaction(mutation_mutex_, std::defer_lock);
        if (modern) { if (!transaction.try_lock()) return failure("busy", "Another instrument operation is in progress", 409); }
        else transaction.lock();
        PendingActivation pending(activation_pending_);
        std::unique_lock activation(live_mutex_);
        const auto name = safe_filename(std::string(requested) + ".zip");
        const auto archive = settings.instruments / name;
        if (!fs::exists(archive)) return modern ? failure("not_found", "Instrument archive not found", 404) : Reply{404, "Instrument " + std::string(requested) + ".zip not found", {}, {}};
        begin("activate", requested);
        std::optional<std::string> invocation;
        { std::lock_guard lock(state_mutex_); invocation = invocation_; }
        const auto before = bookmark(settings.unit, invocation);
        const auto started = before && before->timestamp ? *before->timestamp : realtime_us();
        try { install(archive, settings, [this](std::string_view value) { phase(value); }); }
        catch (const DeploymentError& error) {
            activation.unlock(); activation_pending_.store(false, std::memory_order_release);
            finish(error.code, error.what(), error.rollback);
            std::cerr << "Instrument installation failed: " << error.what() << '\n';
            return modern ? failure(error.code, error.what(), error.code == "insufficient_space" ? 507 : error.rollback == "not_needed" && error.code != "stop_failed" ? 422 : 500, error.rollback)
                          : Reply{500, "Failed to install instrument " + std::string(requested) + ".zip", {}, {}};
        }
        catch (const std::exception& error) {
            activation.unlock(); activation_pending_.store(false, std::memory_order_release);
            finish("installation_failed", error.what());
            std::cerr << "Instrument installation failed: " << error.what() << '\n';
            return modern ? failure("installation_failed", error.what(), 500) : Reply{500, "Failed to install instrument " + std::string(requested) + ".zip", {}, {}};
        }
        activation.unlock(); activation_pending_.store(false, std::memory_order_release);
        finish({}, "Instrument " + std::string(requested) + " is running");
        if (modern) return json_reply(operation());
        const auto after = bookmark(settings.unit);
        {
            std::lock_guard lock(state_mutex_);
            if (after && !after->invocation.empty()) invocation_ = after->invocation;
            log_cursor_ = before && !before->cursor.empty() ? std::optional(before->cursor) : std::nullopt;
            log_timestamp_ = started;
        }
        return {200, "Instrument " + std::string(requested) + ".zip successfully installed", {}, {}};
    }
public:
    Settings settings;
    std::unique_ptr<EventHub> events;
    void ready() { api_ready_ = monotonic_us(); }
    bool reserve_upload() {
        std::lock_guard lock(upload_mutex_);
        struct statvfs space{};
        if (uploads_ >= 2 || ::statvfs(settings.instruments.c_str(), &space) < 0 ||
            static_cast<std::uint64_t>(space.f_bavail) * space.f_frsize < (uploads_ + 1) * max_upload + 4 * 1024 * 1024) return false;
        ++uploads_; return true;
    }
    void release_upload() { std::lock_guard lock(upload_mutex_); --uploads_; }
    Json health() {
        std::lock_guard lock(health_mutex_);
        if (cached_health_.empty() || monotonic_us() - health_sample_ >= 2000000) {
            cached_health_ = encode(system_health(settings, api_ready_.load(std::memory_order_acquire), api_started_)); health_sample_ = monotonic_us();
        }
        return parse(cached_health_);
    }
    Json details(bool names_only = false) {
        auto running = live(), values = array();
        { std::lock_guard lock(state_mutex_); for (const auto& item : inventory_) append(values, names_only ? text(item.name) : item.json()); }
        if (names_only && running) running = text(field(running.get(), "name"));
        auto result = object(); add(result, "instruments", std::move(values)); add(result, "live_instrument", std::move(running)); return result;
    }
    Json snapshot() {
        auto result = object(); add(result, "type", text("status")); add(result, "instruments", details());
        add(result, "current_instrument", current()); add(result, "operation", operation()); add(result, "health", health()); return result;
    }
    explicit App(Settings config) : settings(std::move(config)) {
        for (const auto& file : fs::directory_iterator(settings.instruments)) {
            if (file.is_regular_file() && file.path().extension() == ".zip") inventory_.push_back(instrument(file.path()));
        }
        if (const auto tail = bookmark(settings.unit)) {
            if (!tail->cursor.empty()) log_cursor_ = tail->cursor;
            if (!tail->invocation.empty()) invocation_ = tail->invocation;
            log_timestamp_ = tail->timestamp;
        }
    }
    Reply uploaded(const fs::path& temporary, std::string_view name) {
        std::lock_guard transaction(mutation_mutex_);
        Archive archive(temporary);
        if (!archive.member("version")) return {400, "Instrument archive missing version file", {}, {}};
        archive.validate();
        const auto target = settings.instruments / name;
        fs::rename(temporary, target);
        auto value = instrument(target);
        {
            std::lock_guard lock(state_mutex_);
            std::erase_if(inventory_, [&](const Instrument& current) { return current.name == value.name; });
            inventory_.push_back(std::move(value));
        }
        if (events) events->wake();
        sync_directory(settings.instruments);
        return {200, "Instrument " + std::string(name) + " uploaded.", {}, {}};
    }
    Reply route(std::string_view path, MHD_Connection* connection, std::string_view method = "GET") {
        if (path == "/api/instruments" || path == "/api/instruments/details") {
            return json_reply(details(!path.ends_with("/details")));
        }
        for (const std::string_view action : {"activate", "preflight", "default", "control"}) {
            const std::string prefix = "/api/instruments/" + std::string(action) + "/";
            if (!path.starts_with(prefix)) continue;
            const auto name = path.substr(prefix.size());
            if (name.empty() || name.contains('/') || safe_filename(name) != name) return failure("invalid_name", "Invalid instrument name", 400);
            if (method != (action == "preflight" ? "GET" : "POST")) return failure("method_not_allowed", "Method not allowed", 405);
            if (action == "activate") return run(name, true);
            std::unique_lock transaction(mutation_mutex_, std::try_to_lock);
            if (!transaction.owns_lock()) return failure("busy", "Another instrument operation is in progress", 409);
            if (action == "control") {
                if (name != "start" && name != "stop" && name != "restart") return failure("invalid_action", "Unknown instrument action");
                PendingActivation pending(activation_pending_); std::unique_lock activation(live_mutex_);
                auto loaded = identity();
                if (!loaded || !fs::is_regular_file(settings.live / "serverd")) return failure("no_instrument", "No instrument is loaded", 409);
                begin(name, field(loaded.get(), "name")); phase(name == "stop" ? "stopping" : name == "restart" ? "restarting" : "starting");
                try {
                    service_action(settings, name, settings.unit);
                    activation.unlock(); activation_pending_.store(false, std::memory_order_release);
                    finish({}, "Instrument " + std::string(name) + " completed"); return json_reply(operation());
                } catch (const std::exception& error) {
                    if (activation.owns_lock()) activation.unlock();
                    activation_pending_.store(false, std::memory_order_release);
                    const auto code = std::string(name) + "_failed"; finish(code, error.what()); return failure(code, error.what(), 500);
                }
            }
            const auto archive = settings.instruments / (std::string(name) + ".zip");
            if (!fs::exists(archive)) return failure("not_found", "Instrument archive not found", 404);
            auto checked = preflight(archive, settings);
            if (action == "preflight") return json_reply(checked.json());
            if (!checked.ready()) return failure(checked.code, checked.message, 422);
            try { atomic_write(settings.instruments / "default", std::string(name) + ".zip\n"); }
            catch (const std::exception& error) {
                { std::lock_guard lock(state_mutex_); for (auto& item : inventory_) item.is_default = is_default(settings.instruments / (item.name + ".zip")); }
                if (events) events->wake();
                return failure("preference_failed", std::string("Could not confirm boot preference durability: ") + error.what() + ". Check the current default before retrying.", 500);
            }
            { std::lock_guard lock(state_mutex_); for (auto& item : inventory_) item.is_default = item.name == name; }
            if (events) events->wake();
            auto result = object(); add(result, "default_instrument", text(name)); return json_reply(std::move(result));
        }
        for (const std::string_view action : {"run", "delete", "commands"}) {
            const std::string prefix = "/api/instruments/" + std::string(action) + "/";
            if (!path.starts_with(prefix)) continue;
            const auto name = path.substr(prefix.size());
            if (name.empty() || name.contains('/')) return {404, "Not found", {}, {}};
            if (action == "run") return run(name);
            const auto zip = safe_filename(std::string(name) + ".zip");
            const auto archive = settings.instruments / zip;
            if (action == "commands") {
                if (!fs::exists(archive)) return {404, "Instrument not found", {}, {}};
                try {
                    const auto commands = Archive(archive).member("drivers.json");
                    if (!commands) return {404, "drivers.json not found", {}, {}};
                    return {200, *commands, "application/json", "attachment; filename=" + safe_filename(std::string(name) + "-drivers.json")};
                } catch (const InvalidArchive&) { return {400, "Invalid instrument archive", {}, {}}; }
            }
            std::lock_guard transaction(mutation_mutex_);
            std::lock_guard lock(state_mutex_);
            const auto item = std::ranges::find(inventory_, name, &Instrument::name);
            if (item == inventory_.end()) return {404, "Instrument not found", {}, {}};
            if (item->is_default) return {200, "Default instrument cannot be removed", {}, {}};
            fs::remove(archive); inventory_.erase(item);
            sync_directory(settings.instruments);
            if (events) events->wake();
            return {200, "Instrument " + zip + " removed.", {}, {}};
        }
        if (path.starts_with("/api/system/")) {
            const auto part = path.substr(12);
            if (part == "status") return json_reply(snapshot());
            if (part == "diagnostics") {
                auto result = snapshot(), logs = array();
                auto manifest = kv_file(settings.manifest), release = kv_file(settings.release);
                add(result, "manifest", manifest ? std::move(manifest) : object()); add(result, "release", release ? std::move(release) : object());
                std::size_t budget = 512 * 1024;
                for (const auto& unit : {settings.unit, std::string("koheron-api.service"), std::string("unzip-default-instrument.service")}) {
                    auto log = object(), entries = array();
                    for (const auto& entry : read_logs(unit, {}, 200).entries) {
                        if (!budget) break;
                        const auto length = std::min(entry.message.size(), std::size_t(4096));
                        auto item = object(); add(item, "ts", entry.timestamp ? integer(*entry.timestamp) : Json{});
                        add(item, "msg", text(entry.message.substr(0, length))); add(item, "truncated", boolean(length < entry.message.size()));
                        const auto encoded_size = encode(item).size();
                        if (encoded_size > budget) break;
                        budget -= encoded_size; append(entries, std::move(item));
                    }
                    add(log, "unit", text(unit)); add(log, "entries", std::move(entries)); append(logs, std::move(log));
                }
                add(result, "logs", std::move(logs)); add(result, "generated_us", integer(realtime_us()));
                if (encode(result).size() > 1024 * 1024) {
                    json_object_object_del(result.get(), "instruments");
                    add(result, "inventory_omitted", boolean(true));
                }
                if (encode(result).size() > 1024 * 1024) {
                    json_object_object_del(result.get(), "manifest"); json_object_object_del(result.get(), "release");
                    add(result, "metadata_omitted", boolean(true));
                }
                if (encode(result).size() > 1024 * 1024) return failure("diagnostics_too_large", "Diagnostic context exceeded the download limit", 500);
                auto reply = json_reply(std::move(result)); reply.disposition = "attachment; filename=koheron-diagnostics.json"; return reply;
            }
            if (part == "build") {
                auto manifest = kv_file(settings.manifest), release = kv_file(settings.release), result = object();
                add(result, "manifest", manifest ? std::move(manifest) : object());
                add(result, "release", release ? std::move(release) : object());
                return json_reply(std::move(result));
            }
            const bool raw = part.ends_with("/raw");
            const auto file = raw ? part.substr(0, part.size() - 4) : part;
            if (file == "manifest" || file == "release") {
                const auto& source = file == "manifest" ? settings.manifest : settings.release;
                if (raw) {
                    const auto bytes = read_file(source);
                    return bytes ? Reply{200, *bytes, "text/plain; charset=utf-8", {}} : Reply{404, std::string(file) + " not found", {}, {}};
                }
                auto value = kv_file(source);
                return value ? json_reply(std::move(value)) : error_reply(std::string(file) + " file not found", 404);
            }
        }
        constexpr std::string_view logs = "/api/logs/koheron";
        if (path == logs || path.starts_with(std::string(logs) + "/")) {
            const auto suffix = path.substr(logs.size());
            const char* requested_cursor = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "cursor");
            std::optional<std::string> cursor = requested_cursor && *requested_cursor ? std::optional(std::string(requested_cursor)) : std::nullopt;
            if (suffix.empty()) {
                int limit = 200;
                if (const char* requested = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "lines")) {
                    const auto value = trim(requested);
                    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), limit);
                    if (error != std::errc{} || end != value.data() + value.size()) return error_reply("invalid lines parameter", 400);
                }
                auto result = read_logs(settings.unit, {}, limit);
                return result.entries.empty() ? error_reply("no logs for " + settings.unit, 404) : json_reply(result.json());
            }
            if (suffix == "/incr") return json_reply(read_logs(settings.unit, cursor, 0).json());
            if (suffix == "/bookmark") {
                const auto tail = bookmark(settings.unit);
                auto result = object(); add(result, "cursor", tail ? text(tail->cursor) : Json{});
                add(result, "ts", tail && tail->timestamp ? integer(*tail->timestamp) : Json{});
                return json_reply(std::move(result));
            }
            if (suffix == "/instrument/incr" || suffix == "/instrument/bookmark") {
                std::lock_guard lock(state_mutex_);
                // Also follows external restarts and a previous instrument's rollback.
                if (const auto tail = bookmark(settings.unit); tail && !tail->invocation.empty()) invocation_ = tail->invocation;
                if (suffix.ends_with("/incr")) {
                    if (!cursor) cursor = log_cursor_;
                    auto result = read_logs(settings.unit, cursor, 0, invocation_, cursor ? std::nullopt : log_timestamp_);
                    if (result.cursor) log_cursor_ = result.cursor;
                    for (const auto& entry : result.entries) if (entry.timestamp) log_timestamp_ = std::max(log_timestamp_.value_or(0), *entry.timestamp);
                    return json_reply(result.json());
                }
                if (!log_cursor_ && !log_timestamp_) {
                    if (const auto tail = bookmark(settings.unit, invocation_)) {
                        log_cursor_ = tail->cursor; log_timestamp_ = tail->timestamp;
                        if (!tail->invocation.empty()) invocation_ = tail->invocation;
                    }
                    log_timestamp_ = std::max(log_timestamp_.value_or(0), realtime_us());
                }
                auto result = object(); add(result, "cursor", log_cursor_ ? text(*log_cursor_) : Json{});
                add(result, "ts", log_timestamp_ ? integer(*log_timestamp_) : Json{});
                return json_reply(std::move(result));
            }
        }
        return {404, "Not found", {}, {}};
    }
};

struct Request {
    App& app;
    MHD_PostProcessor* processor = nullptr;
    fs::path temporary;
    Fd upload;
    std::string filename, field_name;
    std::uint64_t received = 0, uploaded = 0;
    std::optional<Reply> error;
    bool responded = false;
    bool reserved = false;
    explicit Request(App& value) : app(value) {}
    ~Request() {
        if (processor) MHD_destroy_post_processor(processor);
        if (!temporary.empty()) { std::error_code ignored; fs::remove(temporary, ignored); }
        if (reserved) app.release_upload();
    }
};
MHD_Result post(void* context, MHD_ValueKind, const char* key, const char* filename, const char*, const char*,
    const char* bytes, std::uint64_t offset, std::size_t size) noexcept {
    auto& request = *static_cast<Request*>(context);
    try {
        if (!key || !filename || !std::string_view(key).ends_with(".zip")) return MHD_YES;
        if (request.field_name.empty()) {
            request.field_name = key; request.filename = safe_filename(key);
            if (!request.filename.ends_with(".zip")) { request.error = Reply{400, "Invalid instrument filename", {}, {}}; return MHD_NO; }
            if (!request.app.reserve_upload()) { request.error = failure("upload_capacity", "Upload capacity is unavailable; wait for other uploads or free storage", 503); return MHD_NO; }
            request.reserved = true;
            auto [path, fd] = temporary_file(request.app.settings.instruments, ".upload-");
            request.temporary = std::move(path); request.upload = std::move(fd);
        }
        if (request.field_name != key) return MHD_YES;
        if (offset != request.uploaded) throw std::runtime_error("Unexpected multipart offset");
        if (size > max_upload - request.uploaded) { request.error = Reply{413, "Instrument upload exceeds 20 MiB limit", {}, {}}; return MHD_NO; }
        write_all(request.upload.get(), std::span(bytes, size)); request.uploaded += size;
        return MHD_YES;
    } catch (const std::exception& error) {
        std::cerr << "Upload failed: " << error.what() << '\n';
        request.error = Reply{500, "Instrument upload failed.", {}, {}};
        return MHD_NO;
    }
}
MHD_Result respond(MHD_Connection* connection, Reply reply) {
    if (reply.type.empty()) reply.type = "text/html; charset=utf-8";
    auto* response = MHD_create_response_from_buffer(reply.body.size(), reply.body.data(), MHD_RESPMEM_MUST_COPY);
    if (!response) return MHD_NO;
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, reply.type.c_str());
    MHD_add_response_header(response, MHD_HTTP_HEADER_CACHE_CONTROL, "no-store");
    if (!reply.disposition.empty()) MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_DISPOSITION, reply.disposition.c_str());
    const auto result = MHD_queue_response(connection, reply.status, response);
    MHD_destroy_response(response);
    return result;
}
MHD_Result handle(void* context, MHD_Connection* connection, const char* url, const char* method, const char*,
    const char* bytes, std::size_t* size, void** request_context) noexcept {
    try {
        if (!*request_context) {
            auto request = std::make_unique<Request>(*static_cast<App*>(context));
            if (std::string_view(method) == "POST" && std::string_view(url) == "/api/instruments/upload") {
                request->processor = MHD_create_post_processor(connection, 8192, post, request.get());
                if (!request->processor) request->error = Reply{400, "Instrument upload failed.", {}, {}};
            }
            *request_context = request.release();
            return MHD_YES;
        }
        auto& request = *static_cast<Request*>(*request_context);
        if (request.responded) return MHD_YES;
        if (*size != 0) {
            request.received += *size;
            if (request.received > max_upload) request.error = Reply{413, "Instrument upload exceeds 20 MiB limit", {}, {}};
            if (!request.error && request.processor && MHD_post_process(request.processor, bytes, *size) == MHD_NO && !request.error)
                request.error = Reply{400, "Instrument upload failed.", {}, {}};
            *size = 0;
            return MHD_YES;
        }
        request.responded = true;
        if (request.error) return respond(connection, std::move(*request.error));
        if (std::string_view(url) == "/api/events") {
            if (std::string_view(method) != "GET") return respond(connection, failure("method_not_allowed", "Method not allowed", 405));
            return request.app.events->upgrade(connection);
        }
        if (std::string_view(method) == "POST" && std::string_view(url) == "/api/instruments/upload") {
            // Finalization detects a missing closing multipart boundary.
            auto* processor = std::exchange(request.processor, nullptr);
            if (!processor || MHD_destroy_post_processor(processor) == MHD_NO || request.temporary.empty())
                return respond(connection, {400, "Instrument upload failed.", {}, {}});
            if (::fsync(request.upload.get()) < 0) system_error("upload fsync");
            request.upload = Fd{};
            try { return respond(connection, request.app.uploaded(request.temporary, request.filename)); }
            catch (const InvalidArchive& error) { std::cerr << error.what() << '\n'; return respond(connection, {400, "Invalid instrument archive", {}, {}}); }
        }
        if (std::string_view(url) == "/api/instruments/upload") return respond(connection, {405, "Method not allowed", {}, {}});
        const std::string_view path(url);
        const bool command = path.starts_with("/api/instruments/activate/") || path.starts_with("/api/instruments/default/") || path.starts_with("/api/instruments/control/");
        if (command) {
            if (request.received) return respond(connection, failure("unexpected_body", "This command does not accept a request body"));
        } else if (std::string_view(method) != "GET" && std::string_view(method) != "HEAD") return respond(connection, {405, "Method not allowed", {}, {}});
        return respond(connection, request.app.route(path, connection, method));
    } catch (const std::exception& error) {
        std::cerr << "API request failed: " << error.what() << '\n';
        return respond(connection, {500, "Internal server error", {}, {}});
    } catch (...) { return respond(connection, {500, "Internal server error", {}, {}}); }
}
void completed(void*, MHD_Connection*, void** context, MHD_RequestTerminationCode) noexcept {
    delete static_cast<Request*>(*context); *context = nullptr;
}
std::pair<Fd, bool> listen_socket(const fs::path& path) {
    const auto count = sd_listen_fds(1);
    if (count < 0) throw std::runtime_error("Invalid inherited socket");
    if (count > 0) {
        if (count != 1 || sd_is_socket_unix(SD_LISTEN_FDS_START, SOCK_STREAM, 1, nullptr, 0) <= 0)
            throw std::runtime_error("Expected one inherited Unix stream socket");
        return {Fd(SD_LISTEN_FDS_START), true};
    }
    sockaddr_un address{}; address.sun_family = AF_UNIX;
    const std::string name = path.string();
    if (name.size() >= sizeof(address.sun_path)) throw std::runtime_error("Socket path too long");
    std::memcpy(address.sun_path, name.c_str(), name.size() + 1);
    fs::create_directories(path.parent_path());
    // Refuse to remove an existing socket owned by another running daemon.
    Fd listener(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (listener.get() < 0 || ::bind(listener.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) system_error("API socket bind");
    if (::chmod(path.c_str(), 0660) < 0 || ::listen(listener.get(), 100) < 0) system_error("API socket listen");
    return {std::move(listener), false};
}
}

int main(int argc, char** argv) {
    fs::path owned_socket;
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--version") {
            std::cout << "Koheron management API (C++23), libmicrohttpd " << MHD_get_version()
                      << ", libzip " << zip_libzip_version() << '\n';
            return 0;
        }
        Settings settings;
        fs::path socket_path = "/run/koheron-api/app.sock";
        unsigned short port = 0;
        for (int i = 1; i < argc; i += 2) {
            if (i + 1 >= argc) throw std::runtime_error("Missing option value");
            const std::string_view option(argv[i]); const char* value = argv[i + 1];
            if (option == "--instruments") settings.instruments = value;
            else if (option == "--live") settings.live = value;
            else if (option == "--manifest") settings.manifest = value;
            else if (option == "--release") settings.release = value;
            else if (option == "--unit") settings.unit = value;
            else if (option == "--led-unit") settings.led_unit = value;
            else if (option == "--systemctl") settings.systemctl = value;
            else if (option == "--proc") settings.proc = value;
            else if (option == "--socket") socket_path = value;
            else if (option == "--port") {
                unsigned number = 0;
                const std::string_view input(value); const auto [end, error] = std::from_chars(input.data(), input.data() + input.size(), number);
                if (error != std::errc{} || end != input.data() + input.size() || number == 0 || number > 65535) throw std::runtime_error("Invalid port");
                port = static_cast<unsigned short>(number);
            } else throw std::runtime_error("Unknown option " + std::string(option));
        }
        // Block before spawning HTTP threads; shutdown is handled in the main thread.
        sigset_t signals; sigemptyset(&signals); sigaddset(&signals, SIGINT); sigaddset(&signals, SIGTERM);
        if (pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) throw std::runtime_error("Could not block shutdown signals");
        App app(std::move(settings));
        app.events = std::make_unique<EventHub>([&app] { return encode(app.snapshot()); });
        const unsigned flags = MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION | MHD_USE_ITC | MHD_USE_ERROR_LOG | MHD_ALLOW_UPGRADE;
        MHD_Daemon* raw_daemon = nullptr;
        Fd listener;
        if (port == 0) {
            auto [descriptor, inherited] = listen_socket(socket_path);
            listener = std::move(descriptor);
            if (!inherited) owned_socket = socket_path;
            raw_daemon = MHD_start_daemon(flags, 0, nullptr, nullptr, handle, &app,
                MHD_OPTION_LISTEN_SOCKET, listener.get(), MHD_OPTION_CONNECTION_LIMIT, static_cast<unsigned>(32),
                MHD_OPTION_CONNECTION_TIMEOUT, static_cast<unsigned>(60), MHD_OPTION_CONNECTION_MEMORY_LIMIT, static_cast<std::size_t>(32768),
                MHD_OPTION_NOTIFY_COMPLETED, completed, nullptr, MHD_OPTION_END);
        } else {
            sockaddr_in loopback{}; loopback.sin_family = AF_INET; loopback.sin_port = htons(port); loopback.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            raw_daemon = MHD_start_daemon(flags, port, nullptr, nullptr, handle, &app,
                MHD_OPTION_SOCK_ADDR, reinterpret_cast<sockaddr*>(&loopback), MHD_OPTION_CONNECTION_LIMIT, static_cast<unsigned>(32),
                MHD_OPTION_CONNECTION_TIMEOUT, static_cast<unsigned>(60), MHD_OPTION_CONNECTION_MEMORY_LIMIT, static_cast<std::size_t>(32768),
                MHD_OPTION_NOTIFY_COMPLETED, completed, nullptr, MHD_OPTION_END);
        }
        if (!raw_daemon) throw std::runtime_error("Could not start HTTP daemon");
        (void)listener.release(); // libmicrohttpd owns the listening descriptor now.
        std::unique_ptr<MHD_Daemon, decltype(&MHD_stop_daemon)> daemon(raw_daemon, MHD_stop_daemon);
        app.ready();
        sd_notify(0, "READY=1\nSTATUS=Management API is ready");
        std::cout << "Koheron management API ready" << std::endl;
        int signal = 0; sigwait(&signals, &signal);
        sd_notify(0, "STOPPING=1");
        app.events->stop();
        daemon.reset();
        if (!owned_socket.empty()) fs::remove(owned_socket);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Management API failed: " << error.what() << '\n';
        if (!owned_socket.empty()) { std::error_code ignored; fs::remove(owned_socket, ignored); }
        return 1;
    }
}
