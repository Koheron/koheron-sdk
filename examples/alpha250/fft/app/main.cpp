
#include "server/core/lib/app.hpp"

int main() {
    auto& ctx = koheron::start_app();

    auto& fft = ctx.get<FFT>();

    koheron::stop_app();
    return 0;
}