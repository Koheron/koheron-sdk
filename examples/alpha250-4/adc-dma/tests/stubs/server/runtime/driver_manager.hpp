#pragma once
namespace rt {
template<typename Driver> Driver& get_driver() { static Driver driver; return driver; }
}
