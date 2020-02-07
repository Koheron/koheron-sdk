#ifndef __ALPHA_DRIVERS_SPI_CONFIG_HPP__
#define __ALPHA_DRIVERS_SPI_CONFIG_HPP__

#include <context.hpp>

#include <mutex>

class SpiConfig {
  public:
    SpiConfig(Context& ctx)
    : ctl(ctx.mm.get<mem::ps_control>())
    , sts(ctx.mm.get<mem::ps_status>())
    {}

    void lock() {
        mtx.lock();
    }

    void unlock() {
        mtx.unlock();
    }

    // cs_id: Index of the slave on the bus
    // nbytes: Number of bytes send per packet 
    template<uint8_t cs_id, uint8_t nbytes>
    void write_reg(uint32_t data) {
        static_assert(nbytes > 0, "Empty packet");
        static_assert(nbytes <= 4, "Max. 4 bytes per packet");
        static_assert(cs_id <= 2, "Exceeds maximum number of slaves on SPI config bus");

        // Wait for previous write to finish
        while (sts.read<reg::spi_cfg_sts>() == 0);

        constexpr uint32_t TVALID_IDX = 8;
        constexpr uint32_t cmd = (1 << TVALID_IDX) + ((nbytes - 1) << 2) + cs_id;
        ctl.write<reg::spi_cfg_data>(data);
        ctl.write<reg::spi_cfg_cmd>(cmd);
        ctl.clear_bit<reg::spi_cfg_cmd, TVALID_IDX>();
    }

  private:
    Memory<mem::ps_control>& ctl;
    Memory<mem::ps_status>& sts;
    std::mutex mtx;
};

#endif // __ALPHA_DRIVERS_SPI_CONFIG_HPP__