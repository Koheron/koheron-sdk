#ifndef __DRIVER_ID_HPP__
#define __DRIVER_ID_HPP__

#include "meta_utils.hpp"

#include <drivers_table.hpp>

namespace koheron {

inline constexpr driver_id device_num = driver_table::device_num;
inline constexpr auto drivers_names = driver_table::names;
using drivers_tuple_t = typename driver_table::tuple_t;

static_assert(std::tuple_size_v<decltype(drivers_names)> == device_num);

static_assert(std::tuple_size<drivers_tuple_t>::value == device_num - 2, "");

// Driver id from driver type

template<class Driver>
constexpr driver_id driver_id_of = tuple_index_v<std::unique_ptr<Driver>, drivers_tuple_t> + 2;

template<> constexpr driver_id driver_id_of<NoDriver> = 0;
template<> constexpr driver_id driver_id_of<Server> = 1;

// Driver type from driver id

template<driver_id driver>
using device_t = std::remove_reference_t<decltype(*std::get<driver - 2>(std::declval<drivers_tuple_t>()))>;

}

#endif // __DRIVER_ID_HPP__