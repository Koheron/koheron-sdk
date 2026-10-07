from koheron import command
from koheron.phase_noise import SingleChannelPhaseNoiseAnalyzer


class PhaseNoiseAnalyzer(SingleChannelPhaseNoiseAnalyzer):
    @command()
    def set_sampling_frequency(self, rate_hz):
        return self.client.recv_bool()

    @command()
    def get_sampling_frequency(self):
        return self.client.recv_uint32()

    @command(classname="ClockGenerator")
    def set_reference_clock(self, val):
        pass
