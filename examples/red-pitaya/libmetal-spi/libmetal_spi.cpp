#include "libmetal_spi.hpp"
#include "port/xil_io.h"
#include "server/runtime/syslog.hpp"
#include <memory.hpp>
#include <metal/device.h>
#include <metal/irq.h>
#include <metal/sys.h>
#include <xspi.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace {
// On Linux this locks libmetal's IRQ dispatcher and waits for an active handler.
// Never hold the completion mutex while acquiring this lock.
class IrqGuard {
  public:
    IrqGuard() : flags(metal_irq_save_disable()) {}
    ~IrqGuard() { metal_irq_restore_enable(flags); }
    IrqGuard(const IrqGuard&) = delete;
    IrqGuard& operator=(const IrqGuard&) = delete;
  private:
    unsigned int flags;
};

std::string find_spi_device() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::directory_iterator it("/sys/class/uio", ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        std::ifstream address(it->path() / "maps/map0/addr");
        uint64_t base = 0;
        if (!(address >> std::hex >> base) || base != prm::spi_addr) {
            continue;
        }
        const auto device = fs::canonical(it->path() / "device", ec);
        if (!ec) {
            return device.filename().string();
        }
    }
    return {};
}
} // namespace

struct LibmetalSpi::Impl {
    metal_device* device = nullptr;
    XSpi spi{};
    int irq = -1;
    bool metal_started = false;
    bool spi_initialized = false;
    bool registered = false;
    bool ready = false;
    bool active = false; // Protected by libmetal's IRQ lock.
    std::string error;
    std::atomic<uint32_t> interrupts{0};
    uint32_t duration_us = 0;
    std::mutex completion_mutex;
    std::condition_variable completion;
    bool done = false;
    u32 event = 0;

    Impl() {
        const auto name = find_spi_device();
        if (name.empty()) {
            fail("SPI UIO device not found; check the overlay and uio_pdrv_genirq");
            return;
        }
        metal_init_params params = METAL_INIT_DEFAULTS;
        params.log_level = METAL_LOG_WARNING;
        if (metal_init(&params) != 0) {
            fail("metal_init failed");
            return;
        }
        metal_started = true;
        if (metal_device_open("platform", name.c_str(), &device) != 0) {
            fail("metal_device_open failed for " + name);
            return;
        }
        auto* io = metal_device_io_region(device, 0);
        if (io == nullptr || metal_io_phys(io, 0) != prm::spi_addr ||
            metal_io_region_size(io) < prm::spi_range || device->irq_num != 1) {
            fail("SPI UIO mapping or interrupt does not match the FPGA design");
            return;
        }
        // libmetal's Linux backend represents a UIO IRQ by its file descriptor.
        irq = reinterpret_cast<intptr_t>(device->irq_info);
        if (irq < 0) {
            fail("SPI UIO interrupt is unavailable");
            return;
        }
        {
            IrqGuard guard;
            spi_port_bind(io);
            XSpi_Config config{};
            config.BaseAddress = reinterpret_cast<UINTPTR>(metal_io_virt(io, 0));
            config.HasFifos = 1;
            config.NumSlaveBits = 1;
            config.DataWidth = 8;
            config.SpiMode = XSP_STANDARD_MODE;
            config.FifosDepth = prm::spi_fifo_depth;
            if (XSpi_CfgInitialize(&spi, &config, config.BaseAddress) != XST_SUCCESS) {
                fail("XSpi_CfgInitialize failed");
                return;
            }
            spi_initialized = true;
            if (XSpi_SelfTest(&spi) != XST_SUCCESS ||
                XSpi_SetOptions(&spi, XSP_MASTER_OPTION | XSP_LOOPBACK_OPTION |
                                      XSP_MANUAL_SSELECT_OPTION) != XST_SUCCESS ||
                XSpi_SetSlaveSelect(&spi, 1) != XST_SUCCESS) {
                fail("XSpi self-test or loopback configuration failed");
                return;
            }
            XSpi_SetStatusHandler(&spi, this, on_status);
            if (metal_irq_register(irq, on_irq, this) != 0) {
                fail("metal_irq_register failed");
                return;
            }
            registered = true;
            if (XSpi_Start(&spi) != XST_SUCCESS) {
                fail("XSpi_Start failed");
                return;
            }
            ready = true;
        }
        metal_irq_enable(static_cast<unsigned int>(irq));
        logf<INFO>("LibmetalSpi: {} ready, {}-byte FIFO, internal loopback\n",
                   name, prm::spi_fifo_depth);
    }

    ~Impl() {
        if (!metal_started) {
            return;
        }
        if (registered) {
            metal_irq_disable(static_cast<unsigned int>(irq));
        }
        {
            IrqGuard guard;
            if (spi_initialized) {
                XSpi_IntrGlobalDisable(&spi);
                XSpi_Reset(&spi);
            }
            if (registered) {
                metal_irq_unregister(irq);
            }
            spi_port_bind(nullptr);
        }
        // No callback can access the controller or its transfer buffers now.
        if (device != nullptr) {
            metal_device_close(device);
        }
        metal_finish();
    }

    void fail(std::string message) {
        error = std::move(message);
        ready = false;
        logf<ERROR>("LibmetalSpi: {}\n", error);
    }

    static int on_irq(int, void* arg) {
        auto& self = *static_cast<Impl*>(arg);
        ++self.interrupts;
        if (self.active) {
            XSpi_InterruptHandler(&self.spi);
        } else {
            XSpi_IntrClear(&self.spi, XSpi_IntrGetStatus(&self.spi));
        }
        // XSpi acknowledges the IP; libmetal then reads/re-arms the UIO IRQ.
        return METAL_IRQ_HANDLED;
    }

    static void on_status(void* arg, u32 status, unsigned int) {
        auto& self = *static_cast<Impl*>(arg);
        std::lock_guard lock(self.completion_mutex);
        if (!self.done || status != XST_SPI_TRANSFER_DONE) {
            self.event = status;
        }
        self.done = true;
        self.completion.notify_one();
    }

    std::vector<uint8_t> transfer(std::vector<uint8_t>& data, uint32_t timeout_ms) {
        duration_us = 0;
        if (!ready) {
            return {};
        }
        if (data.empty() || data.size() > 65536 || timeout_ms == 0 || timeout_ms > 10000) {
            error = "Expected 1..65536 bytes and a timeout of 1..10000 ms";
            return {};
        }
        error.clear();
        std::vector<uint8_t> received(data.size());
        const auto start = std::chrono::steady_clock::now();
        {
            IrqGuard guard;
            std::lock_guard lock(completion_mutex);
            done = false;
            event = 0;
            active = true;
            if (XSpi_Transfer(&spi, data.data(), received.data(), data.size()) != XST_SUCCESS) {
                active = false;
                XSpi_IntrGlobalDisable(&spi);
                XSpi_Reset(&spi);
                fail("XSpi_Transfer failed; restart the instrument");
                return {};
            }
        }
        bool completed;
        {
            std::unique_lock lock(completion_mutex);
            completed = completion.wait_until(lock, start + std::chrono::milliseconds(timeout_ms),
                                              [this] { return done; });
        }
        {
            // Wait for the entire vendor ISR to finish before returning buffers.
            IrqGuard guard;
            active = false;
            duration_us = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - start).count());
            if (!completed || event != XST_SPI_TRANSFER_DONE) {
                XSpi_IntrGlobalDisable(&spi);
                XSpi_Reset(&spi);
                fail(completed ? "SPI interrupt reported an error; restart the instrument"
                               : "SPI transfer timed out; restart the instrument");
                return {};
            }
        }
        return received;
    }
};

LibmetalSpi::LibmetalSpi() : impl(std::make_unique<Impl>()) {}
LibmetalSpi::~LibmetalSpi() = default;
bool LibmetalSpi::is_ready() { return impl->ready; }
std::string LibmetalSpi::get_error() { return impl->error; }
std::vector<uint8_t> LibmetalSpi::transfer(std::vector<uint8_t> data, uint32_t timeout_ms) {
    return impl->transfer(data, timeout_ms);
}
uint32_t LibmetalSpi::get_interrupt_count() { return impl->interrupts.load(); }
uint32_t LibmetalSpi::get_last_duration_us() { return impl->duration_us; }
