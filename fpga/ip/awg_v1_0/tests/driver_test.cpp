#include "server/drivers/dds/phase-modulator.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <mutex>
#include <limits>
#include <thread>
#include <vector>

// AXI shadow banks and delayed commit acknowledgement. Accessing an absent
// bank fails the test rather than quietly supplying dummy register values.
struct Memory {
    explicit Memory(uint32_t count = 1) : channels(count) {
        for (uint32_t channel = 0; channel < channels; ++channel) {
            registers[channel][0] = 0x504d0001;
            registers[channel][1] = 1023;
            registers[channel][2] = 0x1f0e1830;
            registers[channel][0x18/4] = 16;
            registers[channel][0x1c/4] = channels;
            registers[channel][0x4c/4] = 0x8000; // 50% duty at 48 bits.
            registers[channel][0x50/4] = 1;
        }
    }
    uint32_t channels;
    std::array<std::array<uint32_t,32>,2> registers{};
    std::array<bool,2> stopped{}, pending{};
    std::array<int,2> remaining{};
    std::vector<std::pair<uint32_t,uint32_t>> writes;
    struct Commit {
        uint32_t channel, command;
        std::array<uint32_t,14> settings;
    };
    std::vector<Commit> commits;
    std::mutex mutex;
    template<class T> T read_reg(uint32_t offset) {
        std::lock_guard<std::mutex> lock(mutex);
        const auto channel = offset / 0x1000;
        assert(channel < channels);
        const auto index = (offset % 0x1000) / 4;
        if (index == 0x0c/4) {
            if (pending[channel] && !stopped[channel] && remaining[channel]-- <= 0) pending[channel] = false;
            return pending[channel] ? 1 : 0;
        }
        return registers[channel].at(index);
    }
    template<class T> void write_reg(uint32_t offset,T value) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            const auto channel = offset / 0x1000;
            assert(channel < channels);
            const auto index = (offset % 0x1000) / 4;
            registers[channel].at(index) = value;
            writes.emplace_back(offset,value);
            if(index == 0x10/4) {
                assert(!pending[channel]);
                Commit commit{channel,static_cast<uint32_t>(value),{}};
                std::copy_n(registers[channel].begin()+0x20/4,14,commit.settings.begin());
                commits.push_back(commit);
                pending[channel] = true;
                remaining[channel] = 2;
            }
        }
        std::this_thread::yield(); // Encourage competing configurations to interleave.
    }
};

using dds_pm::Error;
using dds_pm::Settings;
using dds_pm::Waveform;
using Controller = dds_pm::Controller<Memory>;

static void test_discovery_and_banks() {
    for (const uint32_t count : {1u,2u}) {
        Memory memory(count);
        Controller controller(memory);
        assert(controller.valid() && controller.initialization_result());
        assert(controller.channel_count() == count);
        const auto info = controller.channel_info();
        assert(info.phase_width == 48 && info.modulation_width == 24 && info.lut_bits == 14);
        assert(info.prbs_width == 31 && info.output_width == 16 && info.capabilities == 1023);
        assert(controller.default_settings().duty == uint64_t{1}<<47);
        Settings settings = controller.default_settings();
        settings.carrier_increment = 0x123456789abc;
        settings.carrier_phase = 1;
        settings.modulation_increment = 0xabcdef012345;
        settings.modulation_phase = 0xffffffffffff;
        settings.deviation = uint64_t{1}<<48;
        settings.output_enabled = settings.pm_enabled = true;
        settings.waveform = Waveform::triangle;
        for(uint32_t channel=0;channel<count;++channel) {
            assert(controller.configure(channel,settings,true,true));
            assert(memory.registers[channel][0x20/4] == 0x56789abc);
            assert(memory.registers[channel][0x24/4] == 0x1234);
            assert(memory.registers[channel][0x44/4] == 0x10000);
            assert(memory.commits.back().channel == channel && memory.commits.back().command == 7);
            assert(memory.registers[channel][0x54/4] == 0x303);
        }
        auto writes = memory.writes.size();
        const auto result = controller.configure(count,settings);
        assert(result.code == Error::invalid_channel && memory.writes.size() == writes);
        assert(controller.channel_info(count) == dds_pm::ChannelInfo{});
        assert(controller.full_turn(count) == 0);
        settings.deviation = 1;
        assert(controller.configure(settings)); // Channel-zero convenience overload.
        assert(memory.registers[0][0x40/4] == 1 && memory.registers[0][0x44/4] == 0);
        assert(memory.commits.back().command == 1);
        assert(result.code == Error::invalid_channel && std::strlen(result.message()) > 0);
        assert(std::strlen(controller.error()) == 0); // Earlier Result remains intact.
    }
}

static void test_metadata_validation() {
    for (const auto& [index,value] : std::vector<std::pair<uint32_t,uint32_t>>{
        {0,0}, {0x1c/4,0}, {0x1c/4,3}, {2,0x1f0e181f}, {2,0x1f0e1831},
        {2,0x1f0e0f30}, {2,0x1f0e1930}, {2,0x1f071830}, {2,0x1f131830},
        {2,0x080e1830}, {0x18/4,11}, {0x18/4,25}, {1,0x400}}) {
        Memory memory;
        memory.registers[0][index] = value;
        Controller controller(memory);
        assert(!controller.valid() && !controller.initialization_result());
        assert(controller.channel_count() == 0);
        assert(!controller.configure(Settings{}));
        assert(memory.writes.empty());
    }
    for(const auto& [index,value] : std::vector<std::pair<uint32_t,uint32_t>>{
        {0,0}, {0x1c/4,1}, {1,2}, {2,0x1f0e1821}}) {
        Memory memory(2);
        memory.registers[1][index] = value;
        Controller controller(memory);
        assert(!controller.valid());
        assert(!controller.configure(0,Settings{}));
        assert(memory.writes.empty()); // Fail the subsystem coherently.
    }
    Memory memory;
    memory.registers[0][2] = 0x07081021; // Phase 33, mod 16, LUT 8, PN7.
    memory.registers[0][0x18/4] = 14;
    Controller controller(memory);
    assert(controller.valid() && controller.phase_width() == 33);
    assert(controller.full_turn() == uint64_t{1}<<33);
    assert(controller.configure(controller.default_settings()));
}

static void test_setting_validation() {
    Memory memory;
    Controller controller(memory);
    for(auto member : {&Settings::carrier_increment,&Settings::carrier_phase,
                       &Settings::modulation_increment,&Settings::modulation_phase}) {
        Settings settings;
        settings.*member = controller.full_turn();
        assert(controller.configure(settings).code == Error::out_of_range);
    }
    for(auto member : {&Settings::deviation,&Settings::duty}) {
        Settings settings;
        settings.*member = controller.full_turn()+1;
        assert(controller.configure(settings).code == Error::out_of_range);
    }
    Settings settings;
    settings.waveform = static_cast<Waveform>(32);
    assert(controller.configure(settings).code == Error::out_of_range);
    settings.waveform = Waveform::uniform_noise; settings.seed=0;
    assert(controller.configure(settings).code == Error::invalid_noise_seed);
    settings.waveform = Waveform::gaussian_noise;
    assert(controller.configure(settings).code == Error::invalid_noise_seed);
    settings.waveform = Waveform::prbs; settings.seed=0x80000000;
    assert(controller.configure(settings).code == Error::invalid_prbs_seed);
    assert(memory.writes.empty());

    Memory reduced;
    reduced.registers[0][1] = 2;
    Controller small(reduced);
    settings.waveform = Waveform::triangle; settings.seed=1; settings.pm_enabled=true;
    assert(small.configure(settings).code == Error::unavailable_waveform && reduced.writes.empty());
    settings.pm_enabled=false;
    assert(small.configure(settings)); // Unmodulated tone on a reduced build.
    settings.waveform=Waveform::pulse; settings.duty=0;
    assert(small.configure(settings) && reduced.registers[0][0x48/4] == 0);
    settings.duty=small.full_turn();
    assert(small.configure(settings) && reduced.registers[0][0x4c/4] == 0x10000);
}

static void test_timeouts_and_recovery() {
    Memory memory(2);
    Controller controller(memory,std::chrono::milliseconds(10));
    memory.pending[0] = memory.stopped[0] = true;
    const auto pending_result = controller.configure(0,Settings{});
    assert(pending_result.code == Error::pending_commit_timeout);
    assert(memory.writes.empty()); // A prior pending commit must not be overwritten.
    assert(controller.configure(1,Settings{})); // Other bank stays usable.
    memory.stopped[0]=false;
    assert(controller.configure(0,Settings{})); // Resume/acknowledge before writing again.
    memory.stopped[0]=true;
    const auto issued_result = controller.configure(0,Settings{});
    assert(issued_result.code == Error::commit_timeout);
    const auto commit_count = memory.commits.size();
    assert(controller.configure(0,Settings{}).code == Error::pending_commit_timeout);
    assert(memory.commits.size() == commit_count); // No automatic retry of a timed-out commit.
    memory.stopped[0]=false;
    assert(controller.configure(0,Settings{}));
    assert(memory.commits.size() == commit_count+1);
    assert(issued_result.code == Error::commit_timeout);
}

static void test_concurrent_configurations() {
    Memory memory(2);
    Controller controller(memory);
    std::array<std::thread,8> threads;
    for(uint32_t task=0;task<threads.size();++task) {
        threads[task] = std::thread([&,task] {
            const auto marker = task+1;
            Settings settings;
            settings.carrier_increment = marker;
            settings.carrier_phase = marker;
            settings.modulation_increment = marker;
            settings.modulation_phase = marker;
            settings.deviation = marker;
            settings.duty = marker;
            settings.seed = marker;
            for(int i=0;i<8;++i) assert(controller.configure(task%2,settings));
        });
    }
    for(auto& thread : threads) thread.join();
    assert(memory.commits.size() == 64);
    for(const auto& commit : memory.commits) {
        const auto marker = commit.settings[0];
        assert(marker >= 1 && marker <= 8 && (marker-1)%2 == commit.channel);
        for(uint32_t word=0;word<12;word+=2) {
            assert(commit.settings[word] == marker && commit.settings[word+1] == 0);
        }
        assert(commit.settings[12] == marker);
    }
}

static void test_engineering_units_and_updates() {
    Memory memory(2);
    Controller controller(memory);
    dds_pm::SignalSettings signal;
    signal.carrier_phase_deg = -90.0L;
    signal.deviation_deg = 360.0L;
    signal.duty = 1.0L;
    signal.pm_enabled = true;
    assert(controller.configure_signal(1, signal, 250'000'000.0L));
    Settings settings;
    assert(controller.get_settings(1, settings));
    assert(settings.carrier_increment == 0x0a3d70a3d70a);
    assert(settings.carrier_phase == 3*(controller.full_turn()/4));
    assert(settings.deviation == controller.full_turn() && settings.duty == controller.full_turn());
    assert(memory.commits.back().command == 7);
    auto expected = settings;
    assert(controller.mute(1));
    expected.output_enabled = false;
    assert(controller.get_settings(1, settings) && settings == expected);
    assert(memory.commits.back().command == 1); // No oscillator restart on mute.
    assert(controller.set_output_enabled(1, true));
    expected.output_enabled = true;
    assert(controller.set_carrier_frequency(1, 62'500'000.0L, 250'000'000.0L));
    expected.carrier_increment = controller.full_turn()/4;
    assert(controller.set_modulation_frequency(1, 0.0L, 250'000'000.0L));
    expected.modulation_increment = 0;
    assert(controller.set_phase(1, 450.0L));
    expected.carrier_phase = controller.full_turn()/4;
    const auto degree_step = 360.0L / static_cast<long double>(controller.full_turn());
    assert(controller.set_deviation(1, degree_step));
    expected.deviation = 1;
    assert(controller.set_pm_enabled(1, false));
    expected.pm_enabled = false;
    assert(controller.set_waveform(1, dds_pm::Waveform::pulse));
    expected.waveform = dds_pm::Waveform::pulse;
    assert(controller.set_duty(1, 0.25L));
    expected.duty = controller.full_turn()/4;
    assert(controller.set_seed(1, 123));
    expected.seed = 123;
    assert(controller.get_settings(1, settings) && settings == expected);
    assert(memory.commits.back().command == 1); // Widget edits preserve phases.
    assert(controller.restart(1));
    assert(memory.commits.back().command == 7);
    assert(controller.get_settings(1, settings) && settings == expected);
    assert(memory.commits.front().channel == 1);
    for(const auto& commit : memory.commits) assert(commit.channel == 1);
    // Startup mute works without a prior configure and preserves reset defaults.
    assert(controller.mute(0));
    assert(controller.get_settings(0, settings) && !settings.output_enabled);
    assert(settings.duty == controller.full_turn()/2 && settings.seed == 1);
    assert(controller.set_phase(0, -degree_step/2.0L));
    assert(controller.get_settings(0, settings) && settings.carrier_phase == controller.full_turn()-1);
    assert(controller.set_deviation_word(0, 17));
    assert(controller.set_phase_word(0, 19));
    assert(controller.get_settings(0, settings) && settings.deviation == 17 && settings.carrier_phase == 19);
    const auto before = memory.writes.size();
    assert(controller.set_phase_word(0, controller.full_turn()).code == Error::out_of_range);
    signal.carrier_hz = std::numeric_limits<long double>::quiet_NaN();
    assert(controller.configure_signal(0, signal, 250'000'000.0L).code == Error::invalid_frequency);
    signal.carrier_hz = 10'000'000.0L;
    assert(controller.configure_signal(0, signal, 0.0L).code == Error::invalid_sample_rate);
    signal.carrier_hz = 125'000'000.0L;
    assert(controller.configure_signal(0, signal, 250'000'000.0L).code == Error::invalid_frequency);
    signal.carrier_hz = 10'000'000.0L;
    signal.carrier_phase_deg = std::numeric_limits<long double>::infinity();
    assert(controller.configure_signal(0, signal, 250'000'000.0L).code == Error::invalid_phase);
    signal.carrier_phase_deg = 0.0L;
    signal.deviation_deg = 361.0L;
    assert(controller.configure_signal(0, signal, 250'000'000.0L).code == Error::invalid_deviation);
    signal.deviation_deg = 30.0L;
    signal.duty = -0.1L;
    assert(controller.configure_signal(0, signal, 250'000'000.0L).code == Error::invalid_duty);
    assert(controller.set_deviation(0, -1.0L).code == Error::invalid_deviation);
    assert(controller.set_phase(0, std::numeric_limits<long double>::infinity()).code == Error::invalid_phase);
    assert(controller.set_carrier_frequency(0, -1.0L, 250'000'000.0L).code == Error::invalid_frequency);
    assert(controller.set_duty(0, 1.01L).code == Error::invalid_duty);
    assert(controller.set_duty(0, std::numeric_limits<long double>::quiet_NaN()).code == Error::invalid_duty);
    assert(controller.set_waveform(0, static_cast<dds_pm::Waveform>(10)).code == Error::out_of_range);
    assert(memory.writes.size() == before);
    assert(controller.set_waveform(0, dds_pm::Waveform::prbs));
    const auto seed_before = memory.writes.size();
    assert(controller.set_seed(0, 0).code == Error::invalid_prbs_seed);
    assert(memory.writes.size() == seed_before);
}

static void test_concurrent_partial_updates() {
    Memory memory;
    Controller controller(memory);
    assert(controller.configure(controller.default_settings()));
    std::thread frequency([&] {assert(controller.set_carrier_frequency(0, 62'500'000.0L, 250'000'000.0L));});
    std::thread phase([&] {assert(controller.set_phase(0, 90.0L));});
    frequency.join();phase.join();
    Settings settings;
    assert(controller.get_settings(0,settings));
    assert(settings.carrier_increment == controller.full_turn()/4);
    assert(settings.carrier_phase == controller.full_turn()/4); // Neither update lost.
}

int main() {
    test_discovery_and_banks();
    test_metadata_validation();
    test_setting_validation();
    test_timeouts_and_recovery();
    test_concurrent_configurations();
    test_engineering_units_and_updates();
    Memory units_memory;
    Controller units_controller(units_memory, 250'000'000.0L);
    assert(units_controller.configure_signal(0, dds_pm::SignalSettings{}));
    assert(units_controller.set_carrier_frequency(0, 5'000'000.0L));
    Controller invalid_rate(units_memory, 0.0L);
    assert(!invalid_rate.valid() && invalid_rate.initialization_result().code == Error::invalid_sample_rate);
    test_concurrent_partial_updates();
}
