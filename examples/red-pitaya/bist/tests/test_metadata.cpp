#include "../metadata.hpp"
#include <cassert>
#include <iostream>

int main()
{
    const auto fields = bist::release_fields(
        "# Comment\nPRETTY_NAME=\"Ubuntu 26.04.1 LTS\"\n"
        "RELEASE=red-pitaya-fft\r\nBUILD_ID=20261014.hash-dirty\n"
        "DESCRIPTION='value=with spaces'\nESCAPED=\"quote\\\" and \\\\ and \\$HOME\"\n"
        "EMPTY=\nignored\n");
    assert(bist::field(fields, "PRETTY_NAME") == "Ubuntu 26.04.1 LTS");
    assert(bist::field(fields, "RELEASE") == "red-pitaya-fft");
    assert(bist::field(fields, "DESCRIPTION") == "value=with spaces");
    assert(bist::field(fields, "ESCAPED") == "quote\" and \\ and $HOME");
    assert(bist::field(fields, "EMPTY") == "unavailable");
    assert(bist::field(fields, "MISSING") == "unavailable");
    assert(bist::fields_json({{"NAME", "quote\""}}) == "{\"NAME\":\"quote\\\"\"}");
    assert(bist::fields_json({}) == "{}");
    std::cout << "metadata helpers PASS\n";
}
