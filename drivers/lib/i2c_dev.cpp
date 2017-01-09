// (c) Koheron

#include "i2c_dev.hpp"

#include <context.hpp>
#include <core/syslog.tpp> // FIXME Not very nice ...

I2cDev::I2cDev(Context& ctx_)
: ctx(ctx_)
{}

int I2cDev::init() {
    if (fd < 0) {
        fd = open("/dev/i2c-0", O_RDWR);

        if (fd < 0) {
            ctx.log<ERROR>("I2cDev: Cannot open /dev/i2c-0\n");
            return -1;
        }
    }

    ctx.log<INFO>("I2cDev: /dev/i2c-0 opened\n");
    return 0;
}

int I2cDev::set_address(int32_t addr) {
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        ctx.log<ERROR>("I2cDev: Failed to acquire bus access and/or "
                       "talk to slave with address %u.\n", addr);
        return -1;
    }

    return 0;
}