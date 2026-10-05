#pragma once
namespace services {
template<class T> T& require() { static T value; return value; }
}
