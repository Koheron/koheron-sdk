// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_IO
#define SCICPP_CORE_IO

#include "scicpp/core/macros.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/tuple.hpp"
#include "scicpp/core/units/quantity.hpp"

#include <Eigen/Dense>
#include <algorithm>
#include <any>
#include <array>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace scicpp {

// A dictionary mapping column number to a function that will parse the
// column string into the desired value.

using ConvertersDict =
    std::map<signed_size_t, std::function<std::any(const char *str)>>;

// template <typename... Converters>
// using ConvertersDict = std::tuple<std::pair<signed_size_t, Converters...>>;

using FiltersDict = std::map<signed_size_t, std::function<bool(std::any)>>;

// template <typename... Filters>
// using FiltersDict = std::tuple<std::pair<signed_size_t, Filters...>>;

// template <typename... DataTypes>
// using FiltersDict =
//     std::map<signed_size_t, std::function<bool(std::variant<DataTypes...>)>>;

namespace detail {

template <typename T>
auto to_number([[maybe_unused]] const char *str) {
    // Should use from_chars when implemented
    // https://en.cppreference.com/w/cpp/utility/from_chars
    if constexpr (units::is_quantity_v<T>) {
        return T(to_number<typename T::value_type>(str));
    } else if constexpr (std::is_floating_point_v<T>) {
        return static_cast<T>(std::atof(str));
    } else if constexpr (std::is_integral_v<T>) {
        return static_cast<T>(std::atoi(str));
    } else if constexpr (meta::is_complex_v<T>) {
        using scal_t = typename T::value_type;
        scal_t x, y;

        if (std::sscanf(str, "%lf+%lfj", &x, &y) > 0) {
            return std::complex(x, y);
        }

        // Invalid input
        const auto nan = std::numeric_limits<scal_t>::quiet_NaN();
        return std::complex(nan, nan);
    } else if constexpr (meta::is_string_v<T>) {
        return T{str};
    } else {
        return T{};
    }
}

template <typename T>
auto to_number(const std::string &str) {
    return to_number<T>(str.data());
}

using token_t = std::pair<signed_size_t, std::string>;

template <typename DataType>
std::optional<DataType> convert(const token_t &tok,
                                const ConvertersDict &converters,
                                const FiltersDict &filters) {
    const auto &[col_id, token] = tok;
    DataType res{};

    if (!converters.empty()) {
        const auto converter = converters.find(col_id);

        if (converter != converters.end()) {
            res = std::any_cast<DataType>(converter->second(token.data()));
        } else {
            res = to_number<DataType>(token);
        }
    } else {
        res = to_number<DataType>(token);
    }

    if (filters.empty()) {
        return res;
    }

    const auto filter = filters.find(col_id);

    if (filter != filters.end()) {
        return filter->second(res) ? std::make_optional(res) : std::nullopt;
    }

    return res;
}

template <class TokenOp>
inline void iterate_line(const char *str,
                         char sep,
                         const std::vector<signed_size_t> &usecols,
                         TokenOp op) {
    std::stringstream ss(str);
    std::string tok;
    signed_size_t col_idx = 0;
    std::size_t usecol_idx = 0;

    while (std::getline(ss, tok, sep)) {
        if (!tok.empty()) {
            if (usecols.empty() || usecols[usecol_idx] == col_idx) {
                op(tok, col_idx);
                ++usecol_idx;

                if (usecol_idx == usecols.size()) {
                    break;
                }
            }

            ++col_idx;
        }
    }
}

template <typename DataType>
auto push_string_to_vector(std::vector<DataType> &vec,
                           const char *str,
                           char sep,
                           const ConvertersDict &converters,
                           const FiltersDict &filters = {},
                           const std::vector<signed_size_t> &usecols = {}) {
    signed_size_t len = 0;

    iterate_line(
        str, sep, usecols, [&](const auto &token, [[maybe_unused]] auto idx) {
            const auto res =
                convert<DataType>({len, token}, converters, filters);

            if (res.has_value()) {
                vec.push_back(res.value());
            }

            ++len;
        });

    return len;
}

template <class tokens_t>
auto tokenize(tokens_t &tokens,
              const char *str,
              char sep,
              const std::vector<signed_size_t> &usecols) {
    std::size_t tok_idx = 0;

    iterate_line(str, sep, usecols, [&](const auto &token, auto idx) {
        // FIXME Is this really an error, or should we just return here ?
        scicpp_require(tok_idx < tokens.size() &&
                       "Too many columns in delimiter-separated file");
        tokens[tok_idx] = {idx, token};
        ++tok_idx;
    });

    scicpp_require(tok_idx == tokens.size() &&
                   "Too few columns in delimiter-separated file");
    return tokens;
}

template <class tokens_t, class Tuple, std::size_t... I>
auto tokens_to_tuple(const tokens_t &tokens,
                     const ConvertersDict &converters,
                     const FiltersDict &filters,
                     Tuple /*unused*/,
                     std::index_sequence<I...> /*unused*/) {
    return std::make_tuple(convert<std::tuple_element_t<I, Tuple>>(
        std::get<I>(tokens), converters, filters)...);
}

template <typename... DataTypes>
auto load_line_to_tuple(const char *str,
                        char sep,
                        const ConvertersDict &converters,
                        const FiltersDict &filters,
                        const std::vector<signed_size_t> &usecols) {
    std::array<token_t, sizeof...(DataTypes)> tokens{};
    tokenize(tokens, str, sep, usecols);
    return tokens_to_tuple(tokens,
                           converters,
                           filters,
                           std::tuple<DataTypes...>{},
                           std::make_index_sequence<sizeof...(DataTypes)>{});
}

template <class LineOp>
void iterate_file(const std::filesystem::path &fname,
                  char comments,
                  signed_size_t skiprows,
                  signed_size_t max_rows,
                  LineOp op) {
    std::ifstream file(fname);
    signed_size_t skip = skiprows;

    if (file.is_open()) {
        std::string line;

        while (skip > 0) {
            --skip;
            std::getline(file, line);
        }

        while (std::getline(file, line) && max_rows != 0) {
            if (line[0] != comments) {
                op(line);
                --max_rows;
            }
        }

        file.close();
    }
}

// Load all the data in a single vector and return the number of columns.
template <typename DataType>
auto loadtxt_to_vector(const std::filesystem::path &fname,
                       char comments,
                       char delimiter,
                       signed_size_t skiprows,
                       const std::vector<signed_size_t> &usecols,
                       const ConvertersDict &converters,
                       const FiltersDict &filters,
                       signed_size_t max_rows) {
    std::vector<DataType> res(0);
    bool is_first_row = true;
    signed_size_t num_cols = 0;

    iterate_file(fname, comments, skiprows, max_rows, [&](auto line) {
        signed_size_t line_cols = detail::push_string_to_vector(
            res, line.data(), delimiter, converters, filters, usecols);

        if (is_first_row) {
            num_cols = line_cols;
        } else {
            scicpp_require(line_cols == num_cols &&
                           "Delimiter-separated file must have the same number "
                           "of columns on each line");
        }

        is_first_row = false;
    });

    return std::make_tuple(res, num_cols);
}

} // namespace detail

//---------------------------------------------------------------------------------
// fromstring
//---------------------------------------------------------------------------------

template <typename DataType = double>
auto fromstring(const char *str,
                char sep,
                const ConvertersDict &converters = {}) {
    std::vector<DataType> res;
    detail::push_string_to_vector(res, str, sep, converters);
    return res;
}

template <typename DataType = double>
auto fromstring(const std::string &str,
                char sep,
                const ConvertersDict &converters = {}) {
    return fromstring(str.data(), sep, converters);
}

//---------------------------------------------------------------------------------
// loadtxt
//---------------------------------------------------------------------------------

template <class EigenMatrix,
          std::enable_if_t<meta::is_eigen_matrix_v<EigenMatrix>, int> = 0>
EigenMatrix loadtxt(const std::filesystem::path &fname,
                    char comments,
                    char delimiter,
                    signed_size_t skiprows,
                    const std::vector<signed_size_t> &usecols,
                    const ConvertersDict &converters,
                    const FiltersDict &filters,
                    signed_size_t max_rows) {
    using T = typename EigenMatrix::value_type;

    const auto [data, num_cols] = detail::loadtxt_to_vector<T>(fname,
                                                               comments,
                                                               delimiter,
                                                               skiprows,
                                                               usecols,
                                                               converters,
                                                               filters,
                                                               max_rows);

    return Eigen::Map<const Eigen::Matrix<typename EigenMatrix::Scalar,
                                          EigenMatrix::RowsAtCompileTime,
                                          EigenMatrix::ColsAtCompileTime,
                                          Eigen::RowMajor>>(
        data.data(), signed_size_t(data.size()) / num_cols, num_cols);
}

template <typename DataType = double,
          std::enable_if_t<std::is_arithmetic_v<DataType>, int> = 0>
auto loadtxt(const std::filesystem::path &fname,
             char comments,
             char delimiter,
             signed_size_t skiprows,
             const std::vector<signed_size_t> &usecols,
             const ConvertersDict &converters,
             const FiltersDict &filters,
             signed_size_t max_rows) {
    return loadtxt<Eigen::Matrix<DataType, Eigen::Dynamic, Eigen::Dynamic>>(
        fname,
        comments,
        delimiter,
        skiprows,
        usecols,
        converters,
        filters,
        max_rows);
}

namespace detail {

template <class Tuple, std::size_t... I>
auto get_tuple_values(Tuple tup, std::index_sequence<I...> /*unused*/) {
    return std::make_tuple(std::get<I>(tup).value()...);
}

} // namespace detail

template <typename... DataTypes,
          std::enable_if_t<(sizeof...(DataTypes) > 1), int> = 0>
auto loadtxt(const std::filesystem::path &fname,
             char comments,
             char delimiter,
             signed_size_t skiprows,
             const std::vector<signed_size_t> &usecols,
             const ConvertersDict &converters,
             const FiltersDict &filters,
             signed_size_t max_rows) {
    std::vector<std::tuple<DataTypes...>> res;

    detail::iterate_file(fname, comments, skiprows, max_rows, [&](auto line) {
        const auto tup = detail::load_line_to_tuple<DataTypes...>(
            line.data(), delimiter, converters, filters, usecols);

        const bool has_values =
            std::apply([](auto... x) { return (x.has_value() && ...); }, tup);

        if (has_values) {
            res.push_back(detail::get_tuple_values(
                tup, std::make_index_sequence<sizeof...(DataTypes)>{}));
        }
    });

    return res;
}

//---------------------------------------------------------------------------------
// TxtLoader
//
// A more versatile API to call loadtxt with default parameter values
// and explicit parameters naming.
//---------------------------------------------------------------------------------

namespace io {

constexpr bool pack = true;

template <typename T>
auto cast(std::any x) {
    return std::any_cast<T>(x);
}

} // namespace io

template <typename... DataTypes>
class TxtLoader {
    static constexpr auto Ntypes = sizeof...(DataTypes);

  public:
    TxtLoader() = default;

    auto delimiter(char delimiter) {
        m_delimiter = delimiter;
        return *this;
    }

    auto skiprows(int skiprows) {
        m_skiprows = skiprows;
        return *this;
    }

    auto comments(char comments) {
        m_comments = comments;
        return *this;
    }

    auto usecols(const std::vector<signed_size_t> &usecols) {
        scicpp_require(
            ((Ntypes > 1 && usecols.size() == Ntypes) || (Ntypes == 1)) &&
            "Number of used columns must match the number of tuple elements");
        m_usecols = usecols;
        std::sort(m_usecols.begin(), m_usecols.end());
        return *this;
    }

    auto usecols(std::vector<signed_size_t> &&usecols) {
        scicpp_require(
            ((Ntypes > 1 && usecols.size() == Ntypes) || (Ntypes == 1)) &&
            "Number of used columns must match the number of tuple elements");
        m_usecols = std::move(usecols);
        std::sort(m_usecols.begin(), m_usecols.end());
        return *this;
    }

    template <typename... Columns>
    auto usecols(Columns... usecols) {
        static_assert((std::is_integral_v<Columns> && ...),
                      "Used columns must be specified as an integer");
        static_assert(
            (Ntypes > 1 && sizeof...(Columns) == Ntypes) || (Ntypes == 1),
            "Number of used columns must match the number of tuple elements");

        m_usecols.reserve(sizeof...(usecols));
        std::apply([this](auto... x) { (this->m_usecols.push_back(x), ...); },
                   std::forward_as_tuple(usecols...));
        std::sort(m_usecols.begin(), m_usecols.end());
        return *this;
    }

    auto converters(ConvertersDict converters) {
        m_converters = std::move(converters);
        return *this;
    }

    auto max_rows(signed_size_t max_rows) {
        m_max_rows = max_rows;
        return *this;
    }

    auto filters(FiltersDict filters) {
        m_filters = std::move(filters);
        return *this;
    }

    template <bool pack = false>
    auto load(const std::filesystem::path &fname) const {
        const auto data = loadtxt<DataTypes...>(fname,
                                                m_comments,
                                                m_delimiter,
                                                m_skiprows,
                                                m_usecols,
                                                m_converters,
                                                m_filters,
                                                m_max_rows);

        if constexpr (!pack && (Ntypes > 1)) {
            // By default tuple output data are unpacked
            return scicpp::unpack(data);
        } else {
            return data;
        }
    }

    // Return all data in a single std::vector
    auto load_vector(const std::filesystem::path &fname) const {
        // First data type
        using T = std::tuple_element_t<0, std::tuple<DataTypes...>>;

        const auto [data, num_cols] = detail::loadtxt_to_vector<T>(fname,
                                                                   m_comments,
                                                                   m_delimiter,
                                                                   m_skiprows,
                                                                   m_usecols,
                                                                   m_converters,
                                                                   m_filters,
                                                                   m_max_rows);

        if constexpr (Ntypes > 1) {
            return std::tuple{data, num_cols};
        } else {
            return data;
        }
    }

  private:
    char m_delimiter = ' ';
    signed_size_t m_skiprows = 0;
    char m_comments = '#';
    std::vector<signed_size_t> m_usecols = {};
    ConvertersDict m_converters = {};
    FiltersDict m_filters = {};
    signed_size_t m_max_rows = -1;
}; // class TxtLoader

//---------------------------------------------------------------------------------
// savetxt
//---------------------------------------------------------------------------------

template <typename Array, std::enable_if_t<meta::is_iterable_v<Array>, int> = 0>
void savetxt(const std::filesystem::path &fname,
             const Array &X,
             [[maybe_unused]] char delimiter,
             char newline) {
    std::ofstream file(fname);

    for (const auto &x : X) {
        file << x << newline;
    }
}

namespace detail {

template <typename T>
auto format(const T &x) {
    if constexpr (meta::is_complex_v<T>) {
        return std::to_string(x.real()) + std::string("+") +
               std::to_string(x.imag()) + std::string("j");
    } else {
        return x;
    }
}

} // namespace detail

template <
    typename Tuple,
    std::enable_if_t<meta::is_std_tuple_v<Tuple> || meta::is_std_pair_v<Tuple>,
                     int> = 0>
void savetxt(const std::filesystem::path &fname,
             const Tuple &X,
             char delimiter,
             char newline) {
    std::apply(
        [&](auto... xs) {
            (scicpp_require(xs.size() == std::get<0>(X).size() &&
                            "All arrays must have the save size"),
             ...);
        },
        X);

    std::ofstream file(fname);

    for (std::size_t i = 0; i < std::get<0>(X).size(); i++) {
        std::apply(
            [&](auto... xs) {
                constexpr auto ncols = std::tuple_size_v<Tuple>;
                std::size_t n{0};
                ((file << detail::format(xs[i])
                       << (++n != ncols ? delimiter : newline)),
                 ...);
            },
            X);
    }
}

class TxtSaver {
  public:
    TxtSaver() = default;

    auto delimiter(char delimiter) {
        m_delimiter = delimiter;
        return *this;
    }

    auto newline(char newline) {
        m_newline = newline;
        return *this;
    }

    template <typename Tuple,
              std::enable_if_t<meta::is_std_tuple_v<Tuple> ||
                                   meta::is_std_pair_v<Tuple>,
                               int> = 0>
    void save(const std::filesystem::path &fname, const Tuple &X) const {
        savetxt(fname, X, m_delimiter, m_newline);
    }

  private:
    char m_delimiter = ' ';
    char m_newline = '\n';
}; // class TxtSaver

} // namespace scicpp

#endif // SCICPP_CORE_IO