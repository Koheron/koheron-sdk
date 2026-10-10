#include "../native/journal.hpp"
#include <iostream>

// Test the persistent reader against real journal files, without a system bus.
int main(int argc, char** argv) {
    using namespace koheron::management;
    if (argc != 4) return 2;
    try {
        auto stream = follow_logs("koheron-journal-test.service",
            *argv[2] ? std::optional(std::string(argv[2])) : std::nullopt,
            *argv[3] ? std::optional(std::string(argv[3])) : std::nullopt, argv[1]);
        for (std::string line; std::getline(std::cin, line);) {
            const auto batch = stream();
            std::cout << (batch.empty() ? "null" : batch) << std::endl;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
