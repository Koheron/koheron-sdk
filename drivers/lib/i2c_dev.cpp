// (c) Koheron

#include "i2c_dev.hpp"

#include <context.hpp>

I2cDev::I2cDev(Context& ctx_)
: ctx(ctx_)
{}

int I2cDev::init() {
    if (fd < 0) {
        fd = open("/dev/i2c-0", O_RDWR);

        if (fd < 0) {
            ctx.log<ERROR>("Cannot open /dev/i2c-0\n");
            return -1;
        }
    }

    ctx.log<INFO>("/dev/i2c-0 opened\n");
    return 0;
}