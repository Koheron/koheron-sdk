#include <iostream>
#include <fstream>
#include <string>
#include <string_view>

#include "server/core/drivers/drivers_json.hpp"

static std::string pretty(std::string_view s) {
    std::string out;
    out.reserve(s.size() + s.size()/8);
    int indent = 0;
    bool in_str = false;
    bool esc = false;

    auto newline = [&] {
        out.push_back('\n');
        for (int i = 0; i < indent; ++i) out += "  "; // 2 spaces
    };

    for (char c : s) {
        if (in_str) {
            out.push_back(c);
            if (esc) { esc = false; }
            else if (c == '\\') { esc = true; }
            else if (c == '"') { in_str = false; }
            continue;
        }
        switch (c) {
            case '"': in_str = true; out.push_back(c); break;
            case '{': case '[':
                out.push_back(c);
                ++indent; newline();
                break;
            case '}': case ']':
                --indent; newline();
                out.push_back(c);
                break;
            case ',':
                out.push_back(c); newline();
                break;
            case ':':
                out.push_back(c); out.push_back(' ');
                break;
            default:
                if (!(c == ' ' || c == '\n' || c == '\t' || c == '\r'))
                    out.push_back(c);
                break;
        }
    }
    return out;
}

int main(int argc, char** argv) {
    // Build the JSON (instantiates everything via the generated header)
    std::string json = koheron::build_drivers_json();

    bool do_pretty = false;
    const char* out_path = nullptr;

    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "--pretty") do_pretty = true;
        else out_path = argv[i];
    }
    if (do_pretty) json = pretty(json);

    if (out_path) {
        std::ofstream f(out_path, std::ios::binary);
        if (!f) { std::cerr << "Failed to open " << out_path << " for write\n"; return 1; }
        f << json;
    } else {
        std::cout << json << '\n';
    }
    return 0;
}
