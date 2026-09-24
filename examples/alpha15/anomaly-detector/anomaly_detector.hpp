#ifndef ALPHA15_ANOMALY_DETECTOR_HPP
#define ALPHA15_ANOMALY_DETECTOR_HPP

#include "server/hardware/memory_manager.hpp"
#include "server/runtime/syslog.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cerrno>
#include <cstdint>
#include <fcntl.h>
#include <fstream>
#include <mutex>
#include <sys/ioctl.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <vector>
#include <string>

// 128 MiB = 256 DMA packets of 512 KiB. One word stores a sequence tag and
// all 18 ADC bits. The CMA reservation prevents Linux from using those pages.
class AnomalyDetector {
public:
    static constexpr uint32_t sample_rate = 15000000;
    static constexpr uint32_t packet_words = 131072;
    static constexpr uint32_t packet_bytes = 4 * packet_words;
    static constexpr uint32_t packet_count = 256;
    static constexpr uint32_t ring_words = packet_count * packet_words;
    static constexpr uint32_t event_words = 2 * sample_rate;
    static constexpr uint32_t plot_bins = 1000;
    static constexpr uint32_t live_words = 262144; // 17.5 ms, two DAC periods.

    AnomalyDetector()
      : ctl(hw::get_memory<mem::control>()), ps_ctl(hw::get_memory<mem::ps_control>()),
        sts(hw::get_memory<mem::status>()),
        dma(hw::get_memory<mem::dma>()), ram(hw::get_memory<mem::ram_s2mm>()),
        desc(hw::get_memory<mem::ocm_s2mm>()), hp(hw::get_memory<mem::axi_hp2>()),
        sclr(hw::get_memory<mem::sclr>()) {
        ctl.write<reg::capture_enable>(0);
        ps_ctl.write<reg::capture_reset_ps>(0);
        ctl.write<reg::capture_arm>(0);
        ctl.write<reg::model_enable>(0);
        ctl.write<reg::inject_toggle>(0);
        ctl.write<reg::anomaly_threshold>(3000);
        ctl.write<reg::model_commit>(0);
        threshold_ = 3000;

        // Require the full dedicated 128 MiB CMA pool at the DMA address.
        cma_fd_ = ::open("/dev/cma", O_RDWR);
        uint32_t allocation = 128U * 1024U * 1024U;
        int allocation_error = cma_fd_ < 0 ? errno : 0;
        if (!allocation_error && ::ioctl(cma_fd_, _IOWR('Z', 1, uint32_t), &allocation) < 0)
            allocation_error = errno;
        if (allocation_error || allocation != mem::ram_s2mm_addr) {
            error_ = 1; phase_ = 6;
            logf<ERROR>("AnomalyDetector: 128 MiB CMA reservation failed: errno={}, returned=0x{:08x}\n",
                        allocation_error, allocation);
            std::ifstream meminfo("/proc/meminfo");
            for (std::string line; std::getline(meminfo,line);)
                if (line.rfind("Cma",0)==0) logf<ERROR>("{}\n",line);
            return;
        }
        sclr.write<0x8>(0xDF0D);
        sclr.write_mask<0x910, 0x8>(0x8);
        sclr.clear_bit<0x240, 1>();
        hp.clear_bit<0x0, 0>();
        hp.clear_bit<0x14, 0>();
        if (!start_locked()) { error_ = 2; phase_ = 6; return; }
        worker_ = std::thread([this] { service(); });
        learner_ = std::thread([this] { training_loop(); });
    }

    ~AnomalyDetector() {
        quit_ = true;
        if (learner_.joinable()) learner_.join();
        if (worker_.joinable()) worker_.join();
        ctl.write<reg::capture_enable>(0);
        ctl.write<reg::capture_arm>(0);
        ps_ctl.write<reg::capture_reset_ps>(0);
        if (cma_fd_ >= 0 && error_ != 1) {
            dma.set_bit<0x30,2>(); // Stop bus writes before releasing CMA.
            for (unsigned i=0;dma.read_bit<0x30,2>() && i<1000;++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (cma_fd_ >= 0) ::close(cma_fd_);
    }

    // Phase: 0 warming, 1 learned, 2 arming, 3 ready, 4 posttrigger,
    // 5 event held, 6 error, 7 training. Error 0 means no acquisition fault.
    auto get_state() {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t phase = phase_;
        if (running_ && model_ready_ && phase_ != 4 && phase_ != 5 && phase_ != 6) {
            phase = armed_ ? (completed_ * packet_words >= sample_rate ? 3 : 2) : 1;
        }
        uint32_t post = 0;
        if (phase == 4) {
            const uint32_t delta = sts.read<reg::capture_sequence>() - trigger_;
            post = std::min<uint32_t>(1000, uint64_t(delta) * 1000 / sample_rate);
        } else if (phase == 8) post = verification_progress_;
        return std::tuple{phase, error_, uint32_t(completed_), sts.read<reg::anomaly_score>(),
                          threshold_, sts.read<reg::capture_sequence>(), trigger_,
                          sts.read<reg::anomaly_alert>(), post, uint32_t(model_ready_),
                          uint32_t(running_), validation_p999_, suggested_};
    }

    uint32_t set_threshold(uint32_t value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (value < 8 || value > 131071) return 1;
        threshold_ = value;
        threshold_manual_ = true;
        ctl.write<reg::anomaly_threshold>(value);
        return 0;
    }

    uint32_t inject() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!model_ready_ || !armed_ || phase_ == 4 || phase_ == 5 ||
            completed_ * uint64_t(packet_words) < sample_rate) return 1;
        inject_toggle_ ^= 1;
        ctl.write<reg::inject_toggle>(inject_toggle_);
        return 0;
    }

    uint32_t arm() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!model_ready_ || phase_ == 5 || phase_ == 4 || error_) return 1;
        armed_ = true;
        ctl.write<reg::capture_arm>(1);
        phase_ = 2;
        return 0;
    }

    uint32_t try_again() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (error_ || !model_ready_) return 1;
        ctl.write<reg::capture_arm>(0);
        armed_ = false;
        if (!start_locked()) { fail_locked(2); return 2; }
        phase_ = 2;
        armed_ = true;
        ctl.write<reg::capture_arm>(1);
        return 0;
    }

    // Start continuous fitting. The worker uses completed DMA packets, so this
    // call returns immediately and the acquisition never pauses for learning.
    uint32_t learn() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (phase_ == 4 || phase_ == 5 || phase_ == 8) return 4;
        if (error_) return 2;
        learning_ = true;
        if (!model_ready_) phase_ = 7;
        return 0;
    }

    uint32_t set_learning(uint32_t enabled) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (error_) return 2;
        learning_ = enabled != 0;
        if (!learning_ && phase_ == 7) phase_ = 0;
        return 0;
    }

    // Measured samples, software predictions, and errors for a recent 17.5 ms
    // window, followed by four adjacent input samples and their eight features.
    std::vector<uint32_t> get_live_trace() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || completed_*packet_words < live_words) return {};
        std::vector<uint32_t> out(3*plot_bins);
        const uint32_t first=completed_*packet_words-live_words;
        for (uint32_t i=0;i<plot_bins;++i) {
            const uint32_t sequence=first+uint64_t(i)*live_words/plot_bins;
            const int32_t actual=signed_adc(word(sequence));
            const int32_t expected=model_ready_ ? predict(sequence,weights_,bias_) : actual;
            out[i]=uint32_t(actual+131072);
            out[plot_bins+i]=uint32_t(expected+131072);
            out[2*plot_bins+i]=uint32_t(std::abs(actual-expected));
        }
        const uint32_t newest=completed_*packet_words;
        std::array<int32_t,4> history;
        for (unsigned lag=0;lag<4;++lag) {
            const int32_t value=signed_adc(word(newest-1-lag));
            history[lag]=value;
            const int32_t q=value>>2;
            out.push_back(uint32_t(value));
            out.push_back(uint32_t(std::max(0,q)));
            out.push_back(uint32_t(std::max(0,-q)));
        }
        out.push_back(uint32_t(model_ready_ ? predict_from(history,weights_,bias_) : 0));
        return out;
    }

    // Counts, holdout quality and the mean absolute contribution of each
    // hidden unit in ADC counts. Signed weights are sent as two's-complement.
    std::vector<uint32_t> get_diagnostics() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint32_t> out = {uint32_t(learning_),updates_,train_samples_,
            train_mae_,validation_mae_,validation_rmse_,validation_p999_,suggested_,
            uint32_t(bias_)};
        for (auto weight:weights_) out.push_back(uint32_t(weight));
        for (auto contribution:contributions_) out.push_back(contribution);
        out.push_back(training_error_);
        return out;
    }

    // Each bin carries its minimum and maximum 18-bit readings. This retains
    // a microsecond anomaly in the two-second overview even at screen width.
    std::vector<uint32_t> get_live() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (completed_ * packet_words < live_words || !running_) return {};
        return envelope(completed_*packet_words - live_words, live_words);
    }
    std::vector<uint32_t> get_event(uint32_t start, uint32_t span) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (phase_ != 5 || start >= event_words || span == 0) return {};
        span = std::min(span, event_words-start);
        if (start == 0 && span == event_words) return event_overview_;
        return envelope(trigger_ - sample_rate + start, span);
    }
    // Small raw slices make the captured 18-bit samples and sequence tags
    // inspectable without transferring the whole 120 MB event to a browser.
    std::vector<uint32_t> get_event_raw(uint32_t start, uint32_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (phase_ != 5 || start >= event_words || count > 4096) return {};
        count = std::min(count,event_words-start);
        std::vector<uint32_t> out(count);
        const uint32_t first=trigger_-sample_rate+start;
        for (uint32_t i=0;i<count;++i) out[i]=word(first+i);
        return out;
    }
    std::vector<uint32_t> get_model() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint32_t> result;
        for (auto w : weights_) result.push_back(uint32_t(w));
        result.push_back(uint32_t(bias_));
        return result;
    }

private:
    static uint32_t pack(int32_t low, int32_t high) {
        return uint32_t(uint16_t(low)) | (uint32_t(uint16_t(high)) << 16);
    }
    static int32_t signed_adc(uint32_t word) {
        const uint32_t raw=word & 0x3ffff;
        return (raw & 0x20000) ? int32_t(raw)-262144 : int32_t(raw);
    }
    uint32_t word(uint32_t sequence) const {
        return ram.read_reg<uint32_t>((sequence % ring_words) * 4);
    }
    static int32_t predict_from(const std::array<int32_t,4>& history,
                                const std::array<int32_t,8>& weights, int32_t bias) {
        int64_t total=0;
        for (unsigned lag=0;lag<4;++lag) {
            const int32_t q=history[lag] >> 2;
            total+=int64_t(std::max(0,q))*weights[2*lag];
            total+=int64_t(std::max(0,-q))*weights[2*lag+1];
        }
        return std::clamp<int64_t>(((total >> 12)+bias)*4,-131072,131071);
    }
    int32_t predict(uint32_t sequence, const std::array<int32_t,8>& weights,
                    int32_t bias) const {
        std::array<int32_t,4> history;
        for (unsigned lag=0;lag<4;++lag) history[lag]=signed_adc(word(sequence-1-lag));
        return predict_from(history,weights,bias);
    }

    struct Fit {
        std::array<int32_t,8> weights{};
        std::array<uint32_t,8> contributions{};
        int32_t bias=0;
        uint32_t samples=0, train_mae=0, validation_mae=0, validation_rmse=0;
        uint32_t p999=0, suggested=3000, error=0;
    };

    Fit fit_window(uint64_t last) const {
        Fit fit;
        constexpr uint32_t stride=256;
        const uint64_t first=last-sample_rate;
        const uint64_t split=first+3*sample_rate/4;
        double normal[9][10]={};
        int32_t lowest=131071, highest=-131072;
        for (uint64_t j=first+4;j<split;j+=stride) {
            const uint32_t raw=word(j);
            if ((raw>>18)!=(j&0x3fff)) { fit.error=7; return fit; }
            const int32_t actual=signed_adc(raw);
            lowest=std::min(lowest,actual);
            highest=std::max(highest,actual);
            double x[9];
            for (unsigned lag=0;lag<4;++lag) {
                const double v=double(signed_adc(word(j-1-lag)))/131072.0;
                x[2*lag]=std::max(0.0,v);
                x[2*lag+1]=std::max(0.0,-v);
            }
            x[8]=1.0;
            const double y=double(actual)/131072.0;
            for (unsigned a=0;a<9;++a) {
                for (unsigned b=0;b<9;++b) normal[a][b]+=x[a]*x[b];
                normal[a][9]+=x[a]*y;
            }
            ++fit.samples;
        }
        if (highest-lowest<1000) { fit.error=5; return fit; }
        for (unsigned a=0;a<8;++a) normal[a][a]+=0.01;
        for (unsigned a=0;a<9;++a) {
            unsigned pivot=a;
            for (unsigned r=a+1;r<9;++r)
                if (std::abs(normal[r][a])>std::abs(normal[pivot][a])) pivot=r;
            if (std::abs(normal[pivot][a])<1e-9) { fit.error=3; return fit; }
            for (unsigned c=a;c<10;++c) std::swap(normal[a][c],normal[pivot][c]);
            const double scale=normal[a][a];
            for (unsigned c=a;c<10;++c) normal[a][c]/=scale;
            for (unsigned r=0;r<9;++r) if (r!=a) {
                const double factor=normal[r][a];
                for (unsigned c=a;c<10;++c) normal[r][c]-=factor*normal[a][c];
            }
        }
        for (unsigned i=0;i<8;++i) {
            if (!std::isfinite(normal[i][9]) || std::abs(normal[i][9])>=8.0) {
                fit.error=3; return fit;
            }
            fit.weights[i]=int32_t(std::lround(normal[i][9]*4096.0));
        }
        fit.bias=int32_t(std::lround(normal[8][9]*32768.0));

        uint64_t train_sum=0, valid_sum=0, valid_square=0;
        uint32_t train_count=0;
        std::vector<uint32_t> holdout;
        holdout.reserve((last-split)/stride+1);
        std::array<uint64_t,8> contribution_sum{};
        for (uint64_t j=first+4;j<last;j+=stride) {
            const uint32_t raw=word(j);
            if ((raw>>18)!=(j&0x3fff)) { fit.error=7; return fit; }
            std::array<int32_t,4> history;
            for (unsigned lag=0;lag<4;++lag) history[lag]=signed_adc(word(j-1-lag));
            const uint32_t error=uint32_t(std::abs(signed_adc(raw)-predict_from(history,fit.weights,fit.bias)));
            if (j<split) { train_sum+=error; ++train_count; }
            else {
                valid_sum+=error;
                valid_square+=uint64_t(error)*error;
                holdout.push_back(error);
                for (unsigned lag=0;lag<4;++lag) {
                    const int32_t q=history[lag]>>2;
                    contribution_sum[2*lag]+=std::abs((int64_t(std::max(0,q))*fit.weights[2*lag])>>10);
                    contribution_sum[2*lag+1]+=std::abs((int64_t(std::max(0,-q))*fit.weights[2*lag+1])>>10);
                }
            }
        }
        if (holdout.empty()) { fit.error=3; return fit; }
        fit.train_mae=uint32_t(train_sum/train_count);
        fit.validation_mae=uint32_t(valid_sum/holdout.size());
        fit.validation_rmse=uint32_t(std::lround(std::sqrt(double(valid_square)/holdout.size())));
        for (unsigned i=0;i<8;++i) fit.contributions[i]=uint32_t(contribution_sum[i]/holdout.size());
        std::sort(holdout.begin(),holdout.end());
        fit.p999=holdout[holdout.size()*999/1000];
        fit.suggested=std::min<uint32_t>(131071,std::max<uint32_t>(3000,12*fit.p999+64));
        return fit;
    }

    void training_loop() {
        uint64_t last_fit=0;
        uint64_t last_generation=0;
        while (!quit_) {
            uint64_t completed=0, generation=0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (generation_!=last_generation) {
                    last_fit=0;
                    last_generation=generation_;
                }
                if (learning_ && running_ && phase_!=4 && phase_!=5 &&
                    !sts.read<reg::anomaly_triggered>() &&
                    completed_*packet_words>=sample_rate+packet_words &&
                    (last_fit==0 || completed_>=last_fit+96)) {
                    completed=completed_;
                    generation=generation_;
                    last_fit=completed;
                }
            }
            if (completed) {
                Fit fit=fit_window(completed*packet_words);
                std::lock_guard<std::mutex> lock(mutex_);
                if (generation==generation_ && learning_ && running_ &&
                    phase_!=4 && phase_!=5 && !sts.read<reg::anomaly_triggered>() &&
                    completed_*packet_words-completed*packet_words<ring_words-packet_words) {
                    training_error_=fit.error;
                    if (!fit.error) {
                        weights_=fit.weights;
                        bias_=fit.bias;
                        contributions_=fit.contributions;
                        train_samples_=fit.samples;
                        train_mae_=fit.train_mae;
                        validation_mae_=fit.validation_mae;
                        validation_rmse_=fit.validation_rmse;
                        validation_p999_=fit.p999;
                        suggested_=fit.suggested;
                        if (!threshold_manual_) {
                            threshold_=suggested_;
                            ctl.write<reg::anomaly_threshold>(threshold_);
                        }
                        ctl.write<reg::model_weight_packed0>(pack(weights_[0],weights_[1]));
                        ctl.write<reg::model_weight_packed1>(pack(weights_[2],weights_[3]));
                        ctl.write<reg::model_weight_packed2>(pack(weights_[4],weights_[5]));
                        ctl.write<reg::model_weight_packed3>(pack(weights_[6],weights_[7]));
                        ctl.write<reg::model_bias>(bias_);
                        commit_^=1;
                        ctl.write<reg::model_commit>(commit_);
                        if (!model_ready_) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            ctl.write<reg::model_enable>(1);
                        }
                        model_ready_=true;
                        ++updates_;
                        if (phase_==7) phase_=1;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    std::vector<uint32_t> envelope(uint32_t start, uint32_t span) const {
        std::vector<uint32_t> out(2*plot_bins);
        for (uint32_t bin=0;bin<plot_bins;++bin) {
            const uint32_t lo = uint64_t(bin)*span/plot_bins;
            const uint32_t hi = std::max(lo+1, uint32_t(uint64_t(bin+1)*span/plot_bins));
            uint32_t low=0x3ffff, high=0;
            for (uint32_t j=lo;j<hi;++j) {
                const uint32_t value = uint32_t(signed_adc(word(start+j))+131072);
                low=std::min(low,value); high=std::max(high,value);
            }
            out[2*bin]=low; out[2*bin+1]=high;
        }
        return out;
    }
    bool start_locked() {
        ctl.write<reg::capture_enable>(0);
        ps_ctl.write<reg::capture_reset_ps>(0);
        running_=false;
        dma.set_bit<0x30,2>();
        for (unsigned i=0;dma.read_bit<0x30,2>();++i) {
            if (i==1000) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        for (uint32_t i=0;i<packet_count;++i) {
            const uint32_t off=64*i;
            desc.write_reg(off+0x00, mem::ocm_s2mm_addr+64*((i+1)%packet_count));
            desc.write_reg(off+0x08, mem::ram_s2mm_addr+i*packet_bytes);
            desc.write_reg(off+0x18, packet_bytes);
            desc.write_reg(off+0x1c, 0);
        }
        completed_=0;
        ++generation_;
        trigger_=0;
        overflow_start_=sts.read<reg::adc_overflow>();
        dma.write<0x38>(mem::ocm_s2mm_addr);
        dma.write<0x30>(1);
        dma.write<0x40>(mem::ocm_s2mm_addr+64*(packet_count-1));
        ps_ctl.write<reg::capture_reset_ps>(1);
        ctl.write<reg::capture_enable>(1);
        running_=true;
        return true;
    }
    void stop_locked() {
        ctl.write<reg::capture_enable>(0);
        ps_ctl.write<reg::capture_reset_ps>(0);
        dma.clear_bit<0x30,0>();
        running_=false;
    }
    void fail_locked(uint32_t code) {
        error_=code; phase_=6; stop_locked();
        logf<ERROR>("AnomalyDetector: acquisition error {}\n",code);
    }
    void service() {
        while (!quit_) {
            bool verify=false;
            uint32_t verify_first=0, verify_overflow=0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (running_) {
                    if (sts.read<reg::adc_overflow>() != overflow_start_) fail_locked(4);
                    else if (dma.read<0x34>() & 0x770) fail_locked(5);
                    else {
                        for (unsigned n=0;n<packet_count;++n) {
                            const uint32_t off=64*(completed_%packet_count);
                            const uint32_t status=desc.read_reg(off+0x1c);
                            if (!(status&0x80000000)) break;
                            if ((status&0x3ffffff)!=packet_bytes) { fail_locked(6); break; }
                            desc.write_reg(off+0x1c,0);
                            dma.write<0x40>(mem::ocm_s2mm_addr+off);
                            ++completed_;
                        }
                        if (running_ && sts.read<reg::anomaly_triggered>() && phase_ != 4) {
                            trigger_=sts.read<reg::anomaly_trigger>();
                            phase_=4;
                        }
                        if (running_ && sts.read<reg::capture_done>()) {
                            const uint32_t count=sts.read<reg::capture_sequence>();
                            const uint64_t written=completed_*packet_words;
                            uint64_t target=(written & ~0xffffffffULL) | count;
                            if (target < written) target += 1ULL << 32;
                            if (written >= target) {
                                running_=false;
                                dma.clear_bit<0x30,0>();
                                phase_=8;
                                verification_progress_=0;
                                verify=true;
                                verify_first=trigger_-sample_rate;
                                verify_overflow=overflow_start_;
                            }
                        }
                    }
                }
            }
            if (verify) {
                // A stopped DMA ring is stable. Scan outside the state lock so
                // progress and controls stay responsive during verification.
                bool good=true;
                std::vector<uint32_t> overview(2*plot_bins);
                for (uint32_t bin=0;bin<plot_bins;++bin) {
                    overview[2*bin]=0x3ffff;
                    overview[2*bin+1]=0;
                }
                for (uint32_t i=0;i<event_words;++i) {
                    const uint32_t seq=verify_first+i;
                    const uint32_t raw=word(seq);
                    if ((raw>>18)!=(seq&0x3fff)) { good=false; break; }
                    const uint32_t bin=uint64_t(i)*plot_bins/event_words;
                    const uint32_t value=uint32_t(signed_adc(raw)+131072);
                    overview[2*bin]=std::min(overview[2*bin],value);
                    overview[2*bin+1]=std::max(overview[2*bin+1],value);
                    if ((i&0xffff)==0) verification_progress_=uint64_t(i)*1000/event_words;
                }
                std::lock_guard<std::mutex> lock(mutex_);
                if (good && sts.read<reg::adc_overflow>()==verify_overflow) {
                    event_overview_=std::move(overview);
                    verification_progress_=1000;
                    phase_=5;
                } else fail_locked(7);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    hw::Memory<mem::control>& ctl;
    hw::Memory<mem::ps_control>& ps_ctl;
    hw::Memory<mem::status>& sts;
    hw::Memory<mem::dma>& dma;
    hw::Memory<mem::ram_s2mm>& ram;
    hw::Memory<mem::ocm_s2mm>& desc;
    hw::Memory<mem::axi_hp2>& hp;
    hw::Memory<mem::sclr>& sclr;
    std::mutex mutex_;
    std::thread worker_;
    std::thread learner_;
    std::atomic<bool> quit_{false};
    int cma_fd_=-1;
    uint64_t completed_=0;
    uint32_t overflow_start_=0, error_=0, phase_=0;
    uint32_t threshold_=3000, trigger_=0, inject_toggle_=0, commit_=0;
    uint32_t validation_p999_=0, suggested_=3000;
    uint32_t train_samples_=0, train_mae_=0, validation_mae_=0, validation_rmse_=0;
    uint32_t updates_=0, training_error_=0;
    std::atomic<uint32_t> verification_progress_{0};
    uint64_t generation_=0;
    std::array<int32_t,8> weights_{};
    std::array<uint32_t,8> contributions_{};
    int32_t bias_=0;
    std::vector<uint32_t> event_overview_;
    bool running_=false, armed_=false, model_ready_=false;
    bool learning_=false, threshold_manual_=false;
};

#endif
