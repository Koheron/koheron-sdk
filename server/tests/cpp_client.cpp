// Run with: python3 server/tests/test_cpp_client.py
#include "server/client/koheron-client.hpp"
#include <cassert>

template<typename F>
void expect_op_error(F&& f) {
    bool rejected = false;
    try { f(); }
    catch (const op_check_error&) { rejected = true; }
    assert(rejected);
}

int main(int argc, char** argv) {
    assert(argc == 3);
    const int port = std::stoi(argv[1]);
    const int http_port = std::stoi(argv[2]);
    std::string host = "127.0.0.1";
    auto client = connect_instrument(host, "", false, http_port, port);
    // The helper's returned client must own the hostname independently.
    host[0] = 'X';
    client->refresh_server_context();

    assert(context_has_class(client->server_context(), "TestDriver"));
    assert(!context_has_class(client->server_context(), "MissingDriver"));
    assert(context_has_class(ServerContext{"1", R"([{"class" : "Empty","id":3,"functions":[]}])"}, "Empty"));
    assert(!context_has_class(ServerContext{"1", R"({"name":"TestDriver"})"}, "TestDriver"));
    assert(check_instrument_loaded("127.0.0.1", "test", {"TestDriver"}, http_port, port));
    assert(!check_instrument_loaded("127.0.0.1", "test", {"MissingDriver"}, http_port, port));

    // Rejected requests must never be sent: the following valid exchange
    // also checks that argument errors leave the wire stream intact.
    expect_op_error([&] { client->call_rt(2, 0); });
    expect_op_error([&] { client->call_rt(2, 0, 1.0f); });
    expect_op_error([&] { client->call_rt(2, 0, uint32_t{1}, uint32_t{2}); });
    expect_op_error([&] { client->call_rt(99, 0); });
    client->call_rt(2, 0, uint32_t{42});
    client->call_by_name("TestDriver::get_value");
    assert(client->recv_by_name<uint32_t>("TestDriver::get_value") == 42);
    client->call_by_name<KOHERON_OP(TestDriver, get_value)>();
    assert((client->recv_by_name<KOHERON_OP(TestDriver, get_value), uint32_t>() == 42));
    client->call_n<op::TestDriver::get_value>("get_value");
    assert((client->recv_n<op::TestDriver::get_value, uint32_t>("get_value") == 42));
}
