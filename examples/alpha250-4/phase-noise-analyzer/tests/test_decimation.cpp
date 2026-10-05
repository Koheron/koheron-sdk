#include "../phase-decimation.hpp"
#include <scicpp/signal.hpp>
#include <cassert>
#include <iostream>
#include <random>

using namespace scicpp::operators;
using namespace pna_dsp;

void check_filter(const std::array<float, 1000>& input) {
    constexpr auto coefficients = make_lowpass_fir<fir_ntaps>(fir_cutoff);
    const auto reference = scicpp::signal::lfilter(coefficients, std::array<float, 1>{1.f}, input);
    const auto output = decimate_by_10_fir_exact<float, 90>(input);
    for (std::size_t i = 0; i < output.size(); ++i)
        assert(std::abs(output[i] - reference[10*i + fir_ntaps/2]) < 2e-5f);
}

template<std::size_t Size, std::size_t Level>
void check_compensation() {
    const auto& weights = decimation_compensation<Size, Level>();
    assert((&weights == &decimation_compensation<Size, Level>()));
    for (float fs : {1e6f, 5e6f, 12.5e6f}) {
        const float segment_fs = fs / std::pow(10.f, float(Level));
        for (std::size_t k = 1; k < Size; ++k) {
            float gain = 1.f;
            const float f = float(k) * segment_fs / float(2*(Size-1));
            for (std::size_t stage = 0; stage < Level; ++stage)
                gain *= decimate_by_10_fir_mag2(scicpp::units::dimensionless<float>{f / (fs/std::pow(10.f,float(stage)))});
            const float expected = gain > .8f ? 1.f/gain : 1.f;
            assert(std::abs(weights[k]-expected) < 2e-6f);
        }
    }
}

int main() {
    std::array<float, 1000> input{};
    check_filter(input);
    input.fill(1.f);check_filter(input);
    input.fill(0.f);input[0]=1.f;check_filter(input);
    input[0]=0.f;input[500]=1.f;check_filter(input);
    for (std::size_t i=0;i<input.size();++i) input[i]=float(i)/100.f;
    check_filter(input);
    std::mt19937 random(17);std::uniform_real_distribution<float> noise(-10.f,10.f);
    for (auto& value:input)value=noise(random);
    check_filter(input);
    for (float f:{.001f,.02f,.045f,.25f}) {
        for(std::size_t i=0;i<input.size();++i)input[i]=std::sin(2.f*scicpp::pi<float>*f*float(i));
        check_filter(input);
    }
    using Phase=scicpp::units::radian<float>;
    std::array<Phase,1000> phase{};
    for(std::size_t i=0;i<input.size();++i)phase[i]=Phase{input[i]};
    const auto output=decimate_by_10_fir_exact<Phase,90>(phase);
    const auto scalar=decimate_by_10_fir_exact<float,90>(input);
    for(std::size_t i=0;i<output.size();++i)assert(output[i].eval()==scalar[i]);
    // The SIMD path applies only to float representations; generic double
    // callers retain the scalar arithmetic and its precision.
    std::array<double,1000> doubles{};
    for(std::size_t i=0;i<doubles.size();++i)doubles[i]=double(input[i])+.00000000001*double(i);
    const auto precise=decimate_by_10_fir_exact<double,90>(doubles);
    constexpr auto coefficients=make_lowpass_fir<fir_ntaps>(fir_cutoff);
    for(std::size_t i=0;i<precise.size();++i) {
        double expected=0;
        const auto sample=10*i+fir_ntaps/2;
        for(std::size_t j=0;j<std::min(fir_ntaps,sample+1);++j)expected+=doubles[sample-j]*coefficients[j];
        assert(precise[i]==expected);
    }
    check_compensation<1501,1>();check_compensation<151,2>();
    std::cout << "Retained FIR samples and cached spectral compensation match reference processing\n";
}
