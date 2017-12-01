#ifndef __TESTS_TEST_HPP__
#define __TESTS_TEST_HPP__

#include <array>
#include <vector>
#include <tuple>
#include <string>
#include <cmath>
#include <cstring>
#include <limits>

#include "context.hpp"

class Tests
{
  public:
    Tests(Context&)
    : vector(0)
    {}

    bool set_scalars(uint32_t a, int32_t b, float c, bool d, double e, uint16_t f) {
        return a == 429496729
          && b == -2048
          && std::fabs(c - 3.14159265358F) <= std::numeric_limits<float>::epsilon()
          && d == true
          && std::fabs(e - exp(1)) <= std::numeric_limits<double>::epsilon()
          && f == 42;
    }

    bool set_array(uint32_t u, float f, const std::array<uint32_t, 8192>& arr, double d, int32_t i) {
        if (u != 4223453) return false;
        if (std::fabs(f - 3.14159265358F) > std::numeric_limits<float>::epsilon()) return false;
        if (std::fabs(d - 2.654798454646) > std::numeric_limits<double>::epsilon()) return false;
        if (i != -56789) return false;

        for (unsigned int j=0; j<8192; j++) {
            if (arr[j] != j) return false;
        }

        return true;
    }

    std::vector<float>& get_vector() {
        vector.resize(10);

        for (unsigned int i=0; i<vector.size(); i++) {
            vector[i] = float(i) * float(i) * float(i);
        }

        return vector;
    }

    const std::vector<uint32_t>& get_const_vector() {
        vector_u.resize(42);

        for (unsigned int i=0; i<vector_u.size(); i++) {
            vector_u[i] = i *i;
        }

        return vector_u;
    }

    const auto& get_const_auto_vector() {
        vector_u.resize(100);

        for (unsigned int i=0; i<vector_u.size(); i++) {
            vector_u[i] = 42 * i;
        }

        return vector_u;
    }

    bool set_string(const std::string& str) {
        return str == "Hello World";
    }

    std::string get_string() {
        return "Hello World";
    }

    const std::string& get_const_string() {
        return const_string;
    }

    std::string get_json() {
        return "{\"date\":\"20/07/2016\",\"machine\":\"PC-3\",\"time\":\"18:16:13\",\"user\":\"thomas\",\"version\":\"0691eed\"}";
    }

    auto get_tuple() {
        return std::make_tuple(501762438, 507.3858, 926547.6468507200, true);
    }

  private:
    std::vector<float> vector;
    std::vector<uint32_t> vector_u;
    std::array<uint32_t, 8192> array;
    std::string string;

    std::string const_string = "Hello World const";
};

#endif // __TESTS_TESTS_HPP__
