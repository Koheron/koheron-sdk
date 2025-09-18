/// Service registry
///
/// (c) Koheron

#ifndef __SERVICES_HPP__
#define __SERVICES_HPP__

#include <memory>
#include <atomic>
#include <utility>
#include <cassert>

namespace services {

// One slot per service type T
template<class T>
struct Slot {
    static std::atomic<std::shared_ptr<T>> ptr;
};

template<class T>
std::atomic<std::shared_ptr<T>> Slot<T>::ptr{};

// Install (owning)
template<class T, class... Args>
std::shared_ptr<T> provide(Args&&... args) {
    auto p = std::make_shared<T>(std::forward<Args>(args)...);
    Slot<T>::ptr.store(p, std::memory_order_release);
    return p;
}

// Install an existing shared_ptr (owning or aliasing)
template<class T>
void provide(std::shared_ptr<T> p) {
    Slot<T>::ptr.store(std::move(p), std::memory_order_release);
}

// Non-throwing lookup
template<class T>
std::shared_ptr<T> get() {
    return Slot<T>::ptr.load(std::memory_order_acquire);
}

// Require the service (asserts if missing)
template<class T>
T& require() {
    auto p = get<T>();
    assert(p && "Service not installed");
    return *p;
}

// Uninstall / free (drops the shared_ptr)
template<class T>
void remove() {
    Slot<T>::ptr.store({}, std::memory_order_release);
}

} // namespace services

#endif // __SERVICES_HPP__