#pragma once
#include "server/runtime/services.hpp"
namespace rt {
class DriverManager {
  public:
    template<class T> T& get() { return services::require<T>(); }
};
template<class T> T& get_driver() { return services::require<T>(); }
}
