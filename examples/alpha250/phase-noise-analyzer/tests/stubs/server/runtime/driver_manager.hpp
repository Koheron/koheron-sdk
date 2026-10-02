#pragma once
namespace rt {
template<class T> T& get_driver() { static T value; return value; }
}
