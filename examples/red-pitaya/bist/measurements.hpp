#ifndef KOHERON_BIST_MEASUREMENTS_HPP
#define KOHERON_BIST_MEASUREMENTS_HPP
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
namespace bist
{
using Clock = std::chrono::steady_clock;
inline double seconds(Clock::time_point start)
{
    return std::chrono::duration<double>(Clock::now() - start).count();
}
inline std::string utc_timestamp(std::time_t seconds, long nanoseconds)
{
    std::tm tm{};
    gmtime_r(&seconds, &tm);
    char date[32];
    std::strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S", &tm);
    std::ostringstream out;
    out << date << '.' << std::setw(6) << std::setfill('0') << nanoseconds / 1000 << 'Z';
    return out.str();
}
inline std::string quote(std::string_view value)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value)
    {
        if (c == '"' || c == '\\')
            out << '\\' << c;
        else if (c < 32)
            out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << unsigned(c)
                << std::dec;
        else
            out << c;
    }
    out << '"';
    return out.str();
}
inline std::string number(double value)
{
    if (!std::isfinite(value))
        return "null";
    std::ostringstream out;
    out << std::setprecision(12) << value;
    return out.str();
}
inline std::string read_file(const std::string& path)
{
    std::ifstream in(path);
    return {std::istreambuf_iterator<char>(in), {}};
}
inline std::string distribution(std::vector<double> values)
{
    if (values.empty())
        return "null";
    std::sort(values.begin(), values.end());
    auto at = [&](double p)
    { return values[static_cast<size_t>(std::ceil(p * double(values.size()))) - 1]; };
    return "{\"count\":" + std::to_string(values.size()) + ",\"min\":" + number(values.front()) +
           ",\"p50\":" + number(at(.5)) +
           ",\"p95\":" + (values.size() >= 100 ? number(at(.95)) : "null") +
           ",\"p99\":" + (values.size() >= 1000 ? number(at(.99)) : "null") +
           ",\"max\":" + number(values.back()) + "}";
}

// Bounded storage covering every observation, rather than truncating the run.
// Bins below 16 us are 0.25 us wide. Above that, 64 bins per octave bound
// midpoint error to 0.78125%. Min/max always retain the exact observations.
class LatencyHistogram
{
  public:
    void add(double value)
    {
        if (!std::isfinite(value) || value < 0 || value >= std::ldexp(32.0, octaves - 1))
        {
            ++invalid;
            return;
        }
        if (count == 0)
            minimum = maximum = value;
        minimum = std::min(minimum, value);
        maximum = std::max(maximum, value);
        ++count;
        size_t octave = 0;
        double scaled = value;
        while (scaled >= 32 && octave < octaves - 1)
        {
            scaled *= .5;
            ++octave;
        }
        const size_t index = value < 16
                                 ? size_t(value * 4)
                                 : 64 + octave * 64 + size_t(std::min((scaled - 16) * 4, 63.0));
        ++bins[index];
    }
    void merge(const LatencyHistogram& other)
    {
        if (other.count != 0)
        {
            minimum = count == 0 ? other.minimum : std::min(minimum, other.minimum);
            maximum = count == 0 ? other.maximum : std::max(maximum, other.maximum);
        }
        count += other.count;
        invalid += other.invalid;
        for (size_t i = 0; i < bins.size(); ++i)
            bins[i] += other.bins[i];
    }
    double percentile(double p) const
    {
        if (count == 0)
            return std::numeric_limits<double>::quiet_NaN();
        const auto rank = std::max(uint64_t(1), uint64_t(std::ceil(p * double(count))));
        uint64_t accumulated = 0;
        for (size_t i = 0; i < bins.size(); ++i)
        {
            accumulated += bins[i];
            if (accumulated >= rank)
            {
                const double midpoint = i < 64 ? (double(i) + .5) / 4
                                               : (16 + (double((i - 64) % 64) + .5) / 4) *
                                                     std::ldexp(1.0, int((i - 64) / 64));
                return std::clamp(midpoint, minimum, maximum);
            }
        }
        return maximum;
    }
    std::string json() const
    {
        if (count == 0)
            return "null";
        return "{\"count\":" + std::to_string(count) + ",\"min\":" + number(minimum) +
               ",\"p50\":" + number(percentile(.5)) +
               ",\"p95\":" + (count >= 100 ? number(percentile(.95)) : "null") +
               ",\"p99\":" + (count >= 1000 ? number(percentile(.99)) : "null") +
               ",\"max\":" + number(maximum) + ",\"invalid\":" + std::to_string(invalid) +
               ",\"method\":\"log_linear_histogram\",\"relative_error_max\":0.0078125,"
               "\"minimum_bin_us\":0.25}";
    }
    uint64_t count = 0, invalid = 0;
    double minimum = 0, maximum = 0;

  private:
    static constexpr size_t octaves = 24;
    std::array<uint64_t, 64 + 64 * octaves> bins{};
};
inline uint32_t pattern(uint32_t index, uint32_t seed) { return (index * 0x9e3779b9U) ^ seed; }
inline uint8_t serial_pattern(size_t index, unsigned endpoint)
{
    return static_cast<uint8_t>((index * 73 + (index >> 8) * 29 + endpoint * 101) ^ (index >> 3));
}
struct Tone
{
    double mean = 0, amplitude = 0, residual_rms = 0, clipping = 0;
};
// Coherent least squares fit at fs/4 (DAC period 32, ADC decimation 8).
// Residual includes noise and distortion, without estimating absolute calibration.
inline Tone fit_tone(const std::vector<int16_t>& data)
{
    Tone result;
    if (data.empty())
        return result;
    double cosine = 0, sine = 0, power = 0;
    for (size_t i = 0; i < data.size(); ++i)
    {
        const double v = data[i];
        result.mean += v;
        power += v * v;
        switch (i % 4)
        {
        case 0:
            cosine += v;
            break;
        case 1:
            sine += v;
            break;
        case 2:
            cosine -= v;
            break;
        case 3:
            sine -= v;
            break;
        default:
            break;
        }
        if (std::abs(int(data[i])) >= 8190)
            result.clipping += 1;
    }
    const double n = double(data.size());
    result.mean /= n;
    cosine *= 2 / n;
    sine *= 2 / n;
    result.amplitude = std::hypot(cosine, sine);
    result.residual_rms = std::sqrt(std::max(0.0, power / n - result.mean * result.mean -
                                                      result.amplitude * result.amplitude / 2));
    return result;
}
inline int16_t adc_signed(uint32_t word, unsigned channel)
{
    const uint32_t code = (word >> (channel * 16)) & 0x3fff;
    return static_cast<int16_t>(code >= 8192 ? int(code) - 16384 : int(code));
}
} // namespace bist
#endif
