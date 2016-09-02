/// Interface for remote SPI control and prototyping

#ifndef __DRIVERS_SPI_SPI_HPP__
#define __DRIVERS_SPI_SPI_HPP__

#include <drivers/lib/memory_manager.hpp>
#include <drivers/lib/spi_dev.hpp>
#include <drivers/memory.hpp>

class Spi
{
  public:
    Spi(MemoryManager& mm) {}

    uint32_t init(uint32_t mode) {
        spi_dev = SpiDev(mode);
        return spi_dev.init();
    }

    uint32_t write(const std::vector<uint32_t>& buffer) {
        return spi_dev.write_buffer(buffer.data(), buffer.size());
    }

  private:
    SpiDev spi_dev;
};

#endif // __DRIVERS_SPI_SPI_HPP__
