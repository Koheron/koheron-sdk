/// (c) Koheron

#include "at93c46d.hpp"

#include <string.h>
#include <thread>
#include <chrono>

At93c46d::At93c46d(Klib::DevMem& dvm_)
: dvm(dvm_)
{
    status = CLOSED;
}

int At93c46d::Open()
{
    if (status == CLOSED) {
        auto ids = dvm.RequestMemoryMaps<2>({{
            { CONFIG_ADDR, CONFIG_RANGE },
            { STATUS_ADDR, STATUS_RANGE }
        }});

        if (dvm.CheckMapIDs(ids) < 0) {
            status = FAILED;
            return -1;
        }
        config_map = ids[0];
        status_map = ids[1];
        status = OPENED;
    }

    return 0;
}