#pragma once
#include "analysis.hpp"
#include "ddr.hpp"
#include "dma.hpp"

namespace bist_full
{
class Analog
{
    Dma &dma;
    hw::Memory<mem::control> &ctl = hw::get_memory<mem::control>();
    hw::Memory<mem::status> &sts = hw::get_memory<mem::status>();
    hw::Memory<mem::bram> &bram = hw::get_memory<mem::bram>();
    hw::Memory<mem::buffers> &ram = hw::get_memory<mem::buffers>();
    std::atomic<bool> &cancel;
    bist::Clock::time_point deadline;
    Emit emit;
    std::string directory;
    std::vector<std::string> artifacts;
    bool available() const { return !cancel && bist::Clock::now() < deadline; }
    struct Capture
    {
        bool ok = false;
        std::vector<double> values;
        std::string artifact;
        uint32_t overflow = 0;
        uint64_t guard_errors = 0;
        std::string evidence() const
        {
            return "\"capture_artifact\":" + bist::quote(artifact) +
                   ",\"fifo_overflow\":" + std::to_string(overflow) +
                   ",\"guard_errors\":" + std::to_string(guard_errors);
        }
    };
    Capture capture(unsigned observe, unsigned drive, const std::vector<int> &waveform,
                    const std::string &label)
    {
        constexpr unsigned count = 8192;
        const bool reset = dma.reset();
        dma.fill(count * 4, 0, 0, 0);
        for (unsigned i = 0; i < 1024; ++i)
            bram.write_reg(4 * i, (uint32_t(waveform[i]) & 0x3fff) << (16 * drive));
        bist::barrier();
        ctl.write<reg::mode>(2);
        ctl.write<reg::decimation>(2);
        ctl.write<reg::samples>(count);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const auto r = dma.transfer({count * 4}, 0, 0, false, true, true);
        const auto token = ctl.read<reg::snapshot>() + 1;
        ctl.write<reg::snapshot>(token);
        bist::barrier();
        const auto end = bist::Clock::now() + std::chrono::milliseconds(10);
        while (sts.read<reg::snapshot_id>() != token && bist::Clock::now() < end)
        {
        }
        Capture result;
        result.overflow = sts.read<reg::overflow>();
        result.guard_errors = dma.guards(count * 4, 0, 0);
        result.ok = reset && r.ok && sts.read<reg::snapshot_id>() == token && !result.overflow &&
                    !result.guard_errors;
        result.artifact = label + ".json";
        artifacts.push_back(result.artifact);
        result.values.resize(count);
        std::vector<uint32_t> packed(count);
        for (unsigned i = 0; i < count; ++i)
        {
            packed[i] = ram.read_reg(Dma::rx_base + 4 * i);
            result.values[i] = bist::adc_signed(packed[i], observe);
        }
        std::ostringstream json;
        json << "{\"drive_channel\":" << drive << ",\"observe_channel\":" << observe
             << ",\"sample_rate_hz\":62500000,\"dac_rate_hz\":125000000,\"capture_ok\":"
             << (result.ok ? "true" : "false") << ",\"dma_ok\":" << (r.ok ? "true" : "false")
             << ",\"fifo_overflow\":" << result.overflow
             << ",\"guard_errors\":" << result.guard_errors << ",\"dac\":[";
        for (unsigned i = 0; i < waveform.size(); ++i)
        {
            if (i)
                json << ',';
            json << waveform[i];
        }
        json << ']';
        for (unsigned channel = 0; channel < 2; ++channel)
        {
            json << ",\"adc" << channel << "\":[";
            for (unsigned i = 0; i < count; ++i)
            {
                if (i)
                    json << ',';
                json << bist::adc_signed(packed[i], channel);
            }
            json << ']';
        }
        json << "}\n";
        std::ofstream file(directory + '/' + result.artifact);
        file << json.str();
        file.close();
        result.ok &= bool(file);
        return result;
    }
    static std::vector<int> tone(unsigned bin, int amplitude, int dc = 0, int clip = 8191)
    {
        std::vector<int> wave(1024);
        for (unsigned i = 0; i < wave.size(); ++i)
            wave[i] = std::clamp(
                int(std::lround(amplitude * std::cos(2 * std::numbers::pi * bin * i / 1024) + dc)),
                -clip, clip);
        return wave;
    }
    static std::string metrics(const Spectral &r)
    {
        return "\"sndr_db\":" + bist::number(r.sndr) + ",\"snr_db\":" + bist::number(r.snr) +
               ",\"thd_dbc\":" + bist::number(r.thd) + ",\"sfdr_db\":" + bist::number(r.sfdr) +
               ",\"enob_at_signal_level\":" + bist::number(r.enob) +
               ",\"amplitude\":" + bist::number(r.amplitude) + ",\"signal_dbfs\":" +
               bist::number(20 * std::log10(std::max(1e-15, r.amplitude) / 8192));
    }

  public:
    Analog(Dma &device, std::atomic<bool> &stop, bist::Clock::time_point end, Emit output,
           std::string path)
        : dma(device), cancel(stop), deadline(end), emit(std::move(output)),
          directory(std::move(path))
    {
    }
    ~Analog()
    {
        std::ofstream file(directory + "/ch_manifest.json");
        file << "{\"files\":[";
        for (size_t i = 0; i < artifacts.size(); ++i)
        {
            if (i)
                file << ',';
            file << bist::quote(artifacts[i]);
        }
        file << "]}\n";
    }
    void exercise(unsigned mask)
    {
        for (unsigned ch = 0; ch < 2 && available(); ++ch)
        {
            if (!(mask & (1U << ch)))
                continue;
            const auto name = "ch" + std::to_string(ch) + "_";
            for (auto [bin, amplitude] : std::array<std::pair<unsigned, int>, 4>{
                     {{13, 4096}, {37, 4096}, {131, 4096}, {211, 4096}}})
            {
                if (!available())
                    return;
                const auto c =
                    capture(ch, ch, tone(bin, amplitude), name + "spectrum_" + std::to_string(bin));
                const auto r = single_tone(c.values, bin * 16);
                const auto clipped = std::count_if(c.values.begin(), c.values.end(),
                                                   [](double x) { return std::abs(x) >= 8190; });
                emit("analog", name + "spectrum_" + std::to_string(bin),
                     c.ok && r.acceptable() && r.amplitude > amplitude * .3 &&
                         r.amplitude < amplitude * 1.7 && clipped == 0,
                     r.sndr, "dB SNDR",
                     "8192 coherent samples: SNR, SNDR, harmonics 2..10, SFDR and ENOB; no sample "
                     "loss",
                     c.evidence() + "," + metrics(r) +
                         ",\"frequency_hz\":" + bist::number(125000000.0 * bin / 1024) +
                         ",\"gain\":" + bist::number(r.amplitude / amplitude) +
                         ",\"clipped_samples\":" + std::to_string(clipped) +
                         ",\"limits\":{\"sndr_min_db\":35,\"sfdr_min_db\":35,\"thd_max_dbc\":-30,"
                         "\"gain_min\":0.3,\"gain_max\":1.7,\"clipped_samples_max\":0}");
            }
            const auto headroom = capture(ch, ch, tone(211, 512), name + "headroom_211");
            const auto hf = single_tone(headroom.values, 211 * 16);
            const auto saturated = std::count_if(headroom.values.begin(), headroom.values.end(),
                                                 [](double x) { return std::abs(x) >= 8190; });
            emit("analog", name + "high_frequency_headroom",
                 headroom.ok && hf.acceptable() && saturated == 0, hf.amplitude / 512,
                 "gain at reduced drive",
                 "25.76 MHz repeated at 512 DAC codes to distinguish headroom from corruption; "
                 "nominal-amplitude verdict retained",
                 headroom.evidence() + "," + metrics(hf) +
                     ",\"clipped_samples\":" + std::to_string(saturated));
            double sums_x = 0, sums_y = 0, xx = 0, xy = 0;
            std::array<double, 7> means{};
            double worst_noise = 0;
            bool ok = true;
            for (unsigned i = 0; i < means.size(); ++i)
            {
                if (!available())
                    return;
                const int code = int(i) * 2000 - 6000;
                const auto c = capture(ch, ch, std::vector<int>(1024, code),
                                       name + "dc_" + std::to_string(code));
                ok &= c.ok;
                const double mean =
                    std::accumulate(c.values.begin(), c.values.end(), 0.0) / c.values.size();
                double power = 0;
                for (double x : c.values)
                    power += (x - mean) * (x - mean);
                worst_noise = std::max(worst_noise, std::sqrt(power / c.values.size()));
                means[i] = mean;
                sums_x += code;
                sums_y += mean;
                xx += double(code) * code;
                xy += code * mean;
            }
            const double gain = (xy - sums_x * sums_y / 7) / (xx - sums_x * sums_x / 7),
                         offset = (sums_y - gain * sums_x) / 7;
            double inl = 0;
            for (unsigned i = 0; i < means.size(); ++i)
                inl = std::max(inl, std::abs(means[i] - (gain * (int(i) * 2000 - 6000) + offset)));
            emit("analog", name + "dc_transfer",
                 ok && gain > .5 && gain < 1.5 && inl < 80 && worst_noise < 30 &&
                     std::is_sorted(means.begin(), means.end()),
                 inl, "ADC codes max residual",
                 "seven DC levels from -6000 to +6000: fitted gain/offset, monotonicity, static "
                 "residual and noise",
                 "\"gain\":" + bist::number(gain) + ",\"offset\":" + bist::number(offset) +
                     ",\"noise_rms\":" + bist::number(worst_noise));
            std::array<double, 3> gains{};
            ok = true;
            for (unsigned i = 0; i < gains.size(); ++i)
            {
                const int amplitude = std::array<int, 3>{512, 4096, 7200}[i];
                const auto c = capture(ch, ch, tone(13, amplitude),
                                       name + "amplitude_" + std::to_string(amplitude));
                const auto r = single_tone(c.values, 13 * 16);
                gains[i] = r.amplitude / amplitude;
                ok &= c.ok && r.amplitude > amplitude * .4 && r.amplitude < amplitude * 1.6 &&
                      std::none_of(
                          c.values.begin(), c.values.end(),
                          [](double x) { return std::abs(x) >= 8190; });
                if (i == 2)
                {
                    const auto stuck = constant_bits(c.values);
                    emit(
                        "analog", name + "adc_bit_activity", c.ok && !stuck, stuck,
                        "constant-bit mask",
                        "near-full-scale capture must exercise both values of all 14 ADC data bits",
                        "");
                }
            }
            const auto [low, high] = std::minmax_element(gains.begin(), gains.end());
            emit("analog", name + "amplitude_linearity", ok && *high / *low < 1.05, *high / *low,
                 "gain ratio",
                 "512/4096/7200-code sine amplitudes: relative gain compression and "
                 "near-full-scale clipping",
                 "");
            const auto crossed = capture(ch, 1 - ch, tone(37, 6000), name + "isolation");
            const auto cross = single_tone(crossed.values, 37 * 16);
            const double isolation = db(cross.amplitude * cross.amplitude, 6000.0 * 6000);
            emit("analog", name + "other_dac_isolation", crossed.ok && isolation < -35, isolation,
                 "dB relative to DAC codes",
                 "connected DAC held at zero while unconnected DAC drives a distinct tone; catches "
                 "crossed routing and coupling",
                 "");
            // Real, safe analog fault injection: flatten the DAC sine at +/-1500 codes.
            const auto bad = capture(ch, ch, tone(13, 6000, 0, 1500), name + "clipped");
            const auto clipped = single_tone(bad.values, 13 * 16);
            emit("analog", name + "detect_clipped_waveform",
                 bad.ok && !clipped.acceptable() && clipped.thd > -25, clipped.thd, "dBc THD",
                 "intentionally clipped DAC waveform must fail the ordinary spectral acceptance "
                 "detector",
                 bad.evidence() + "," + metrics(clipped));
            const auto clean = capture(ch, ch, tone(13, 4096), name + "recovery");
            const auto restored = single_tone(clean.values, 13 * 16);
            emit("analog", name + "clipping_recovery", clean.ok && restored.acceptable(),
                 restored.sndr, "dB SNDR",
                 "clean sine recovers ordinary spectral acceptance after deliberate clipping",
                 clean.evidence() + "," + metrics(restored));
            auto wave = tone(17, 2000);
            const auto second = tone(29, 2000);
            for (unsigned i = 0; i < wave.size(); ++i)
                wave[i] += second[i];
            const auto two = capture(ch, ch, wave, name + "two_tone");
            const auto p = spectrum(two.values);
            const double carrier = std::min(p[17 * 16], p[29 * 16]),
                         imd = std::max(p[5 * 16], p[41 * 16]);
            const double imd_db = db(imd, carrier);
            emit("analog", name + "two_tone_imd", two.ok && carrier > 500000 && imd_db < -35,
                 imd_db, "dBc IMD3",
                 "two simultaneous coherent tones; checks both carriers and 2f1-f2/2f2-f1 "
                 "intermodulation products",
                 "");
            // Step response: fold 16 periods; find the rising edge independently of capture phase.
            wave.assign(1024, -2000);
            std::fill(wave.begin() + 512, wave.end(), 2000);
            const auto step = capture(ch, ch, wave, name + "step");
            std::array<double, 512> folded{};
            for (unsigned i = 0; i < step.values.size(); ++i)
                folded[i % 512] += step.values[i] / 16;
            unsigned edge = 0;
            double largest = 0;
            for (unsigned i = 0; i < 512; ++i)
            {
                double difference = 0;
                for (unsigned j = 0; j < 16; ++j)
                    difference += folded[(i + j) % 512] - folded[(i + 511 - j) % 512];
                if (difference > largest)
                {
                    largest = difference;
                    edge = i;
                }
            }
            double before = 0, after = 0;
            for (unsigned i = 32; i < 200; ++i)
            {
                before += folded[(edge + 512 - i) % 512] / 168;
                after += folded[(edge + i) % 512] / 168;
            }
            const double height = after - before;
            double overshoot = 0;
            unsigned settling = 0;
            for (unsigned i = 0; i < 100; ++i)
            {
                const double delta = folded[(edge + i) % 512] - after;
                overshoot = std::max(overshoot, delta / std::max(1.0, height));
                if (std::abs(delta) > std::abs(height) * .02)
                    settling = i + 1;
            }
            emit("analog", name + "step_response",
                 step.ok && height > 2000 && height < 6000 && overshoot < .4 && settling <= 12,
                 settling * 16, "ns to 2% band",
                 "folded bipolar step: plateau height, overshoot and settling; 16 ns sample "
                 "resolution",
                 step.evidence() + ",\"height\":" + bist::number(height) +
                     ",\"overshoot_fraction\":" + bist::number(overshoot) +
                     ",\"limits\":{\"overshoot_max_fraction\":0.4,\"settling_max_ns\":192}");
            // Broadband deterministic sequence; fit unknown cyclic phase and signed gain.
            wave.resize(1024);
            uint32_t lfsr = 0x5a31;
            for (unsigned i = 0; i < 64; ++i)
            {
                lfsr = (lfsr >> 1) ^ ((0U - (lfsr & 1)) & 0x6000U);
                for (unsigned j = 0; j < 16; ++j)
                    wave[16 * i + j] = (lfsr & 1) ? 2000 : -2000;
            }
            const auto prbs = capture(ch, ch, wave, name + "prbs");
            folded.fill(0);
            for (unsigned i = 0; i < prbs.values.size(); ++i)
                folded[i % 512] += prbs.values[i] / 16;
            double best = -1;
            unsigned lag = 0;
            const double mean = std::accumulate(folded.begin(), folded.end(), 0.0) / 512;
            for (unsigned shift = 0; shift < 512; ++shift)
            {
                double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
                for (unsigned i = 0; i < 512; ++i)
                {
                    const double x = wave[(2 * (i + shift)) % 1024], y = folded[i] - mean;
                    sx += x;
                    sy += y;
                    sxx += x * x;
                    syy += y * y;
                    sxy += x * y;
                }
                const double correlation =
                    (sxy - sx * sy / 512) /
                    std::sqrt(std::max(1.0, (sxx - sx * sx / 512) * (syy - sy * sy / 512)));
                if (correlation > best)
                {
                    best = correlation;
                    lag = shift;
                }
            }
            emit("analog", name + "prbs_correlation", prbs.ok && best > .9, best, "correlation",
                 "64-bit deterministic broadband sequence, 16 DAC clocks per bit; signed "
                 "correlation detects polarity and waveform corruption",
                 prbs.evidence() + ",\"cyclic_phase_samples\":" + std::to_string(lag) +
                     ",\"correlation_min\":0.9");
        }
    }
};
} // namespace bist_full
