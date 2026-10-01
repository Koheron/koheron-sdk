"""ALPHA250 internal PM control, with Decimal conversion to native phase words."""
from decimal import Decimal, localcontext, ROUND_HALF_UP
from enum import IntEnum

from koheron import command


class Waveform(IntEnum):
    SINE = 0
    SQUARE = 1
    PULSE = 2
    TRIANGLE = 3
    UP_RAMP = 4
    DOWN_RAMP = 5
    UNIFORM_NOISE = 6
    GAUSSIAN_NOISE = 7
    PRBS = 8
    BPSK = 9


def phase_word(degrees, width=48, deviation=False):
    """Accept strings/Decimal to retain settings finer than float precision."""
    with localcontext() as context:
        context.prec = 80
        value = Decimal(str(degrees))
        if not value.is_finite():
            raise ValueError("Phase must be finite")
        if deviation and not Decimal(0) <= value <= Decimal(360):
            raise ValueError("Deviation must be between 0 and 360 degrees")
        turn = 1 << width
        word = int((value * turn / 360).to_integral_value(rounding=ROUND_HALF_UP))
        return word if deviation else word % turn


def frequency_word(hz, sample_rate, width=48):
    with localcontext() as context:
        context.prec = 80
        value, rate = Decimal(str(hz)), Decimal(str(sample_rate))
        if not value.is_finite() or not rate.is_finite() or not 0 <= value < rate / 2:
            raise ValueError("Frequency must be nonnegative and below Nyquist")
        return int((value * (1 << width) / rate).to_integral_value(rounding=ROUND_HALF_UP))


class PhaseModulator:
    def __init__(self, client):
        self.client = client
        self.sample_rate = self.get_sample_rate()

    @command()
    def get_sample_rate(self):
        return self.client.recv_uint32()

    @command()
    def get_phase_width(self, channel):
        return self.client.recv_uint32()

    @command()
    def get_capabilities(self, channel):
        return self.client.recv_uint32()

    @command()
    def configure_words(self, channel, carrier_increment, carrier_phase,
                        modulation_increment, modulation_phase, deviation, duty,
                        seed, waveform, output_enabled, pm_enabled,
                        restart_carrier, restart_modulation):
        return self.client.recv_bool()

    def configure(self, channel=0, carrier_hz=10_000_000, carrier_phase=0,
                  waveform=Waveform.SINE, modulation_hz=1_000, modulation_phase=0,
                  deviation=30, duty="0.5", seed=1, output_enabled=True,
                  pm_enabled=True, restart=True):
        if channel not in (0, 1):
            raise ValueError("Channel must be 0 or 1")
        width = self.get_phase_width(channel)
        if not 32 <= width <= 48:
            raise ValueError("Unsupported phase width")
        waveform = Waveform(waveform)
        if pm_enabled and not self.get_capabilities(channel) & (1 << waveform):
            raise ValueError("Modulation source is absent from this FPGA build")
        with localcontext() as context:
            context.prec = 80
            duty_value = Decimal(str(duty))
            if not duty_value.is_finite() or not 0 <= duty_value <= 1:
                raise ValueError("Duty must be between 0 and 1")
            duty_word = int((duty_value * (1 << width)).to_integral_value(rounding=ROUND_HALF_UP))
        if not 0 < seed < 1 << 32:
            raise ValueError("Seed must be a nonzero 32-bit integer")
        result = self.configure_words(
            channel, frequency_word(carrier_hz, self.sample_rate, width),
            phase_word(carrier_phase, width),
            frequency_word(modulation_hz, self.sample_rate, width),
            phase_word(modulation_phase, width), phase_word(deviation, width, True),
            duty_word, seed, int(waveform), output_enabled, pm_enabled, restart, restart)
        if not result:
            raise RuntimeError("FPGA rejected the PM configuration; inspect the server log")


if __name__ == "__main__":
    import argparse
    from koheron import connect
    parser = argparse.ArgumentParser(description="Generate a 10 MHz carrier with 1 kHz, +/-30 degree PM")
    parser.add_argument("host")
    args = parser.parse_args()
    generator = PhaseModulator(connect(args.host, name="phase-modulator"))
    generator.configure()
