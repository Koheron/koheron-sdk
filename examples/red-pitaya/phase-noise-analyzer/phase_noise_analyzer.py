from koheron.phase_noise import SingleChannelPhaseNoiseAnalyzer


class PhaseNoiseAnalyzer(SingleChannelPhaseNoiseAnalyzer):
    def get_phase_snapshot(self):
        # Preserve the existing Red Pitaya client return shape.
        sequence, bits, scale, valid, samples = super().get_phase_snapshot()
        return (sequence, bits, scale, valid), samples
