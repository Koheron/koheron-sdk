/// Utilities to compute FFT windows
///
/// (c) Koheron

#include <array>
#include <vector>

#ifndef __SERVER_FFT_WINDOWS_HPP__
#define __SERVER_FFT_WINDOWS_HPP__

class FFTwindow {
  public:
    enum WindowType {
        rectangular = 0,
        hann = 1,
        flat_top = 2,
        blackman_harris = 3
    };

    FFTwindow(WindowType wintype_, size_t fft_size)
    : window(fft_size)
    , wintype(wintype_)
    {
        compute_window();
    }

    void set_window(WindowType wintype_) {
        wintype = wintype_;
        compute_window();
    }

    void resize(size_t fft_size) {
        window.resize(fft_size);
        compute_window();
    }

    const auto& values() const {
        return window;
    }

    auto& operator[](size_t i) const {
        return window[i];
    }

    auto W1() const {
        return _W1;
    }

    auto W2() const {
        return _W2;
    }

  private:
    std::vector<float> window;
    float _W1, _W2; // Window correction factors
    WindowType wintype;

    void compute_window() {
        // Cosine sum window coefficients
        // https://en.wikipedia.org/wiki/Window_function
        constexpr std::array<std::array<float, 6>, 4> fft_windows_coeffs = {{
            {1.0, 0, 0, 0, 0, 1.0},                      // Rectangular
            {0.5, 0.5, 0, 0, 0, 1.0},                    // Hann
            {1.0, 1.93, 1.29, 0.388, 0.028, 0.2},        // Flat top
            {0.35875, 0.48829, 0.14128, 0.01168, 0, 1.0} // Blackman-Harris
        }};

        float sign;
        auto a = fft_windows_coeffs[wintype];
        float res1 = 0;
        float res2 = 0;

        size_t fft_size = window.size();

        for (size_t i=0; i<fft_size; i++) {
            window[i] = 0;

            for (size_t j=0; j<(a.size() - 1); j++) {
                j % 2 == 0 ? sign = 1.0 : sign = -1.0;
                window[i] += sign * a[j] * std::cos(2 * float(M_PI) * i * j / float(fft_size - 1));
            }

            window[i] *= a[a.size() - 1]; // Scaling

            res1 += window[i];
            res2 += window[i] * window[i];
        }

        _W1 = res1 * res1;
        _W2 = res2;
    }

}; // class FFTwindow

#endif // __SERVER_FFT_WINDOWS_HPP__