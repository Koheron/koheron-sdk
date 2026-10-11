// Freestanding real FFT and bit-exact chirp reference for the browser worker.
// Pack pairs into an N/2 complex FFT: workspace is four bytes per real sample.
#include <stdint.h>

extern unsigned char __heap_base;
static uint32_t size, adc_base, work_base, lut_base;
static uint64_t lanes[16], coefficient, phase;
static uint32_t source_shift;
void set_source_shift(uint32_t shift) { source_shift = shift <= 2 ? shift : 0; }

uint32_t layout(uint32_t n) {
    if (n < 32 || n > 67108864 || (n & (n - 1))) return 0;
    size = n;
    adc_base = ((uint32_t)(uintptr_t)&__heap_base + 63u) & ~63u;
    work_base = adc_base + 2 * n;
    lut_base = work_base + 4 * n;
    phase = 0;
    return lut_base + 65536 * 2;
}
uint32_t adc_address(void) { return adc_base; }
uint32_t work_address(void) { return work_base; }
uint32_t lut_address(void) { return lut_base; }
void seed(uint32_t lane, uint32_t lo, uint32_t hi) {
    if (lane < 16) lanes[lane] = ((uint64_t)hi << 32) | lo;
}
void set_coefficient(uint32_t lo, uint32_t hi) {
    coefficient = ((uint64_t)hi << 32) | lo;
}

// High 64 bits of a 64x64 multiply, rounding at bit 63. No compiler runtime.
static uint64_t rounded_high_product(uint64_t x, uint64_t y) {
    uint64_t a = (uint32_t)x, b = x >> 32, c = (uint32_t)y, d = y >> 32;
    uint64_t ac = a * c, ad = a * d, bc = b * c;
    uint64_t middle = (ac >> 32) + (uint32_t)ad + (uint32_t)bc;
    uint64_t low = (middle << 32) | (uint32_t)ac;
    return b * d + (ad >> 32) + (bc >> 32) + (middle >> 32) + (low >> 63);
}

// Blocks must be called sequentially, beginning at zero; tail is silent.
void reference_block(uint32_t first, uint32_t count, uint32_t samples, uint32_t gain) {
    float *work = (float *)(uintptr_t)work_base;
    const int16_t *lut = (const int16_t *)(uintptr_t)lut_base;
    uint32_t end = first + count;
    if (end > size) end = size;
    for (uint32_t i = first; i < end; ++i) {
        int32_t output = 0;
        if (i < samples) {
            uint32_t lane = i & 15;
            uint64_t state = lanes[lane];
            uint64_t increment = (state >> 16) + ((state >> 15) & 1);
            phase = (phase + increment) & 0xffffffffffffULL;
            lanes[lane] = state + rounded_high_product(state, coefficient);
            uint32_t taper = i < samples - 1 - i ? i : samples - 1 - i;
            if (taper > 262144) taper = 262144;
            int32_t tapered = (int32_t)(((int64_t)lut[phase >> 32] * taper) >> 18);
            output = (tapered * (int32_t)gain) >> 15;
        }
        // Red Pitaya emits bits 15:2 of the shared 16-bit source. Restore that
        // signed 14-bit word to the normalized receiver scale without rounding.
        output &= ~((1 << source_shift) - 1);
        work[i] = (float)output / 32768.0f;
    }
}

void adc_block(uint32_t first, uint32_t count) {
    const int16_t *adc = (const int16_t *)(uintptr_t)adc_base;
    float *work = (float *)(uintptr_t)work_base;
    uint32_t end = first + count;
    if (end > size) end = size;
    for (uint32_t i = first; i < end; ++i) work[i] = (float)adc[i] / 32768.0f;
}

static uint32_t reverse(uint32_t v) {
    v = ((v >> 1) & 0x55555555u) | ((v & 0x55555555u) << 1);
    v = ((v >> 2) & 0x33333333u) | ((v & 0x33333333u) << 2);
    v = ((v >> 4) & 0x0f0f0f0fu) | ((v & 0x0f0f0f0fu) << 4);
    v = ((v >> 8) & 0x00ff00ffu) | ((v & 0x00ff00ffu) << 8);
    return (v >> 16) | (v << 16);
}

static inline void first_pair(float *w, float ar, float ai, float br, float bi,
                              float cr, float ci, float dr, float di) {
    float x0r = ar + br, x0i = ai + bi, x1r = ar - br, x1i = ai - bi;
    float x2r = cr + dr, x2i = ci + di, x3r = cr - dr, x3i = ci - di;
    w[0] = x0r + x2r; w[1] = x0i + x2i;
    w[2] = x1r + x3i; w[3] = x1i - x3r;
    w[4] = x0r - x2r; w[5] = x0i - x2i;
    w[6] = x1r - x3i; w[7] = x1i + x3r;
}

// Read ADC pairs directly in FFT order and complete the first two stages in
// registers. The raw record stays immutable. This avoids a float conversion
// pass followed by scattered float swaps and another full-array traversal.
void adc_fft_block(uint32_t first, uint32_t count) {
    const int16_t *adc = (const int16_t *)(uintptr_t)adc_base;
    float *w = (float *)(uintptr_t)work_base;
    uint32_t m = size / 2, bits = 0;
    for (uint32_t v = m; v > 1; v >>= 1) ++bits;
    uint32_t end = first + count;
    if (end > m) end = m;
    for (uint32_t i = first; i < end; i += 4) {
        uint32_t j = 2 * (reverse(i) >> (32 - bits));
        float ar = adc[j] / 32768.0f, ai = adc[j+1] / 32768.0f;
        float br = adc[j+m] / 32768.0f, bi = adc[j+m+1] / 32768.0f;
        float cr = adc[j+m/2] / 32768.0f, ci = adc[j+m/2+1] / 32768.0f;
        float dr = adc[j+3*m/2] / 32768.0f, di = adc[j+3*m/2+1] / 32768.0f;
        first_pair(w + 2*i, ar, ai, br, bi, cr, ci, dr, di);
    }
}
void reorder_block(uint32_t first, uint32_t count) {
    float *w = (float *)(uintptr_t)work_base;
    uint32_t m = size / 2, bits = 0;
    for (uint32_t v = m; v > 1; v >>= 1) ++bits;
    uint32_t end = first + count;
    if (end > m) end = m;
    for (uint32_t i = first; i < end; ++i) {
        uint32_t j = reverse(i) >> (32 - bits);
        if (i < j) {
            float re = w[2*i], im = w[2*i+1];
            w[2*i] = w[2*j]; w[2*i+1] = w[2*j+1];
            w[2*j] = re; w[2*j+1] = im;
        }
    }
}

// One radix-2 stage. Double twiddles avoid drift on the longest transforms.
void fft_stage(uint32_t length, double step_re, double step_im) {
    float *w = (float *)(uintptr_t)work_base;
    uint32_t half = length / 2, m = size / 2;
    for (uint32_t first = 0; first < m; first += length) {
        double re = 1.0, im = 0.0;
        for (uint32_t j = 0; j < half; ++j) {
            uint32_t a = 2 * (first + j), b = 2 * (first + j + half);
            double br = w[b] * re - w[b+1] * im;
            double bi = w[b] * im + w[b+1] * re;
            float ar = w[a], ai = w[a+1];
            w[a] = (float)(ar + br); w[a+1] = (float)(ai + bi);
            w[b] = (float)(ar - br); w[b+1] = (float)(ai - bi);
            double next_re = re * step_re - im * step_im;
            im = re * step_im + im * step_re; re = next_re;
        }
    }
}

// Fuse two radix-2 stages so each group is loaded and stored once. Keep the
// intermediate float rounding and double twiddles of the single-stage path.
static void fft_pair_range(uint32_t length, double step_re, double step_im, uint32_t begin, uint32_t end) {
    float *w = (float *)(uintptr_t)work_base;
    uint32_t quarter = length / 4;
    // The first pair has only unit and -i twiddles.
    if (length == 4) {
        for (uint32_t i = 2 * begin; i < 2 * end; i += 8) {
            float ar = w[i], ai = w[i+1], br = w[i+2], bi = w[i+3];
            float cr = w[i+4], ci = w[i+5], dr = w[i+6], di = w[i+7];
            first_pair(w + i, ar, ai, br, bi, cr, ci, dr, di);
        }
        return;
    }
    double step2_re = step_re * step_re - step_im * step_im;
    double step2_im = 2 * step_re * step_im;
    for (uint32_t first = begin; first < end; first += length) {
        double re = 1, im = 0, re2 = 1, im2 = 0;
        for (uint32_t j = 0; j < quarter; ++j) {
            uint32_t a = 2 * (first + j), b = a + 2 * quarter;
            uint32_t c = b + 2 * quarter, d = c + 2 * quarter;
            double br = w[b] * re2 - w[b+1] * im2;
            double bi = w[b] * im2 + w[b+1] * re2;
            double dr = w[d] * re2 - w[d+1] * im2;
            double di = w[d] * im2 + w[d+1] * re2;
            float ar = w[a], ai = w[a+1], cr = w[c], ci = w[c+1];
            float x0r = (float)(ar + br), x0i = (float)(ai + bi);
            float x1r = (float)(ar - br), x1i = (float)(ai - bi);
            float x2r = (float)(cr + dr), x2i = (float)(ci + di);
            float x3r = (float)(cr - dr), x3i = (float)(ci - di);
            double vr = x2r * re - x2i * im;
            double vi = x2r * im + x2i * re;
            // The second half's twiddle is -i times the first half's.
            double ur = x3r * im + x3i * re;
            double ui = x3i * im - x3r * re;
            w[a] = (float)(x0r + vr); w[a+1] = (float)(x0i + vi);
            w[b] = (float)(x1r + ur); w[b+1] = (float)(x1i + ui);
            w[c] = (float)(x0r - vr); w[c+1] = (float)(x0i - vi);
            w[d] = (float)(x1r - ur); w[d+1] = (float)(x1i - ui);
            double next_re = re * step_re - im * step_im;
            im = re * step_im + im * step_re; re = next_re;
            double next_re2 = re2 * step2_re - im2 * step2_im;
            im2 = re2 * step2_im + im2 * step2_re; re2 = next_re2;
        }
    }
}

void fft_stage_pair(uint32_t length, double step_re, double step_im) {
    fft_pair_range(length, step_re, step_im, 0, size / 2);
}

// Complete early stages in cache-sized blocks before traversing the full record.
// Values and rounding follow the same paired butterflies; only scheduling changes.
uint32_t fft_prefix_length(void) {
    uint32_t length = 4;
    while (length < 256 && length * 4 <= size / 2) length *= 4;
    return length;
}
void fft_prefix_block(uint32_t first, uint32_t count, uint32_t skip_first_pair) {
    static const double real[] = {0, 0.9238795325112867, 0.9951847266721969, 0.9996988186962042};
    static const double imag[] = {-1, -0.3826834323650898, -0.0980171403295606, -0.024541228522912288};
    uint32_t limit = fft_prefix_length();
    uint32_t end = first + count;
    if (end > size / 2) end = size / 2;
    for (uint32_t block = first; block < end; block += limit) {
        for (uint32_t length = skip_first_pair ? 16 : 4, step = skip_first_pair ? 1 : 0; length <= limit; length *= 4, ++step)
            fft_pair_range(length, real[step], imag[step], block, block + limit);
    }
}

// Recover one real-FFT bin from the packed complex transform.
double bin_real(uint32_t bin, double c, double s) {
    float *w = (float *)(uintptr_t)work_base;
    uint32_t m = size / 2, k = bin % m, r = (m - k) % m;
    double ar = w[2*k], ai = w[2*k+1], br = w[2*r], bi = -w[2*r+1];
    return .5 * (ar + br + c * (ai - bi) - s * (ar - br));
}
double bin_imag(uint32_t bin, double c, double s) {
    float *w = (float *)(uintptr_t)work_base;
    uint32_t m = size / 2, k = bin % m, r = (m - k) % m;
    double ar = w[2*k], ai = w[2*k+1], br = w[2*r], bi = -w[2*r+1];
    return .5 * (ai + bi - c * (ar - br) - s * (ai - bi));
}
