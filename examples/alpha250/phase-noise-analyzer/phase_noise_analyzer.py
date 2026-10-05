from koheron import command
from koheron.phase_noise import SingleChannelPhaseNoiseAnalyzer


class PhaseNoiseAnalyzer(SingleChannelPhaseNoiseAnalyzer):
    @command(classname="ClockGenerator")
    def set_reference_clock(self, val):
        pass
