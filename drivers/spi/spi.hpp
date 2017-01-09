/// Interface for remote SPI control and prototyping

#ifndef __DRIVERS_SPI_SPI_HPP__
#define __DRIVERS_SPI_SPI_HPP__

#include <context.hpp>
#include <drivers/lib/spi_dev.hpp>

class Spi
{
  public:
    Spi(Context& ctx_)
    : ctx(ctx_)
    {}

    uint32_t write(const std::vector<uint32_t>& buffer) {
        return ctx.spi.write_buffer(buffer.data(), buffer.size());
    }

  private:
    Context& ctx;
};

#endif // __DRIVERS_SPI_SPI_HPP__
