// Exercise the actual libmetal I/O layer and Xilinx-compatible port on host RAM.
#include "port/xil_io.h"
#include "port/xil_util.h"
#include <metal/io.h>
#include <xstatus.h>

#include <array>
#include <cassert>
#include <chrono>
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    std::array<u32, 8> words{};
    metal_phys_addr_t physical = 0x41E00000;
    metal_io_region io{};
    metal_io_init(&io, words.data(), &physical, sizeof(words), -1, 0, nullptr);
    const auto base = reinterpret_cast<UINTPTR>(words.data());
    spi_port_bind(&io);

    Xil_Out32(base + 4, 0x12345678);
    assert(words[1] == 0x12345678);
    assert(Xil_In32(base + 4) == 0x12345678);
    Xil_Out32(base + sizeof(words) - 4, 0xABCD);
    assert(words.back() == 0xABCD);
    assert(metal_io_phys(&io, 4) == physical + 4);
    assert(Xil_WaitForEvent(base + 4, 0xFF, 0x78, 1000) == XST_SUCCESS);

    const auto start = std::chrono::steady_clock::now();
    assert(Xil_WaitForEvent(base + 4, 0xFF, 0x79, 1000) == XST_FAILURE);
    assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));

    // Bounds/alignment failures must stop before touching unmapped hardware.
    for (const auto address : {base - 4, base + 1, base + sizeof(words)}) {
        const auto pid = fork();
        assert(pid >= 0);
        if (pid == 0) {
            const rlimit limit{0, 0};
            setrlimit(RLIMIT_CORE, &limit);
            Xil_Out32(address, 0);
            _exit(1);
        }
        int status = 0;
        assert(waitpid(pid, &status, 0) == pid);
        assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    }
    spi_port_bind(nullptr);
    metal_io_finish(&io);
}
