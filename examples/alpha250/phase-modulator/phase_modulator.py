"""ALPHA250 internal PM control, with Decimal conversion to native phase words."""
from decimal import Decimal, InvalidOperation, localcontext, ROUND_HALF_UP
from enum import IntEnum
from numbers import Integral

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


def _number(value, name):
    try:
        result = Decimal(str(value))
    except (InvalidOperation, ValueError, TypeError):
        raise ValueError(f"{name} must be a finite number") from None
    if not result.is_finite():
        raise ValueError(f"{name} must be finite")
    return result


def phase_word(degrees, width=48, deviation=False):
    """Accept strings/Decimal to retain settings finer than float precision."""
    with localcontext() as context:
        context.prec = 80
        value = _number(degrees, "Phase")
        if deviation and not Decimal(0) <= value <= Decimal(360):
            raise ValueError("Deviation must be between 0 and 360 degrees")
        turn = 1 << width
        word = int((value * turn / 360).to_integral_value(rounding=ROUND_HALF_UP))
        return word if deviation else word % turn


def frequency_word(hz, sample_rate, width=48):
    with localcontext() as context:
        context.prec = 80
        value, rate = _number(hz, "Frequency"), _number(sample_rate, "Sample rate")
        if rate <= 0 or not 0 <= value < rate / 2:
            raise ValueError("Frequency must be nonnegative and below Nyquist")
        return int((value * (1 << width) / rate).to_integral_value(rounding=ROUND_HALF_UP))


class PhaseModulator:
    def __init__(self, client):
        self.client = client
        self.sample_rate = self.get_sample_rate()
        self.channels = self.get_channel_count()
        if self.channels not in (1, 2):
            raise RuntimeError(self.get_initialization_error() or "FPGA has no usable DDS PM channels")
        if self.sample_rate <= 0:
            raise RuntimeError("Sampling rate must be positive")
        self._info = tuple(self.get_channel_info(channel) for channel in range(self.channels))

    @command()
    def get_initialization_error(self):
        return self.client.recv_string()

    @command()
    def get_channel_info(self, channel):
        return self.client.recv_tuple("IIIIII")

    @command(funcname="get_settings_words")
    def _get_settings_words(self, channel):
        return self.client.recv_tuple("IQQQQQQII??")

    @command()
    def get_error_message(self, code):
        return self.client.recv_string()

    def settings(self, channel=0):
        """Read the acknowledged hardware settings in Hz, degrees and duty fraction."""
        self._validate_channel(channel)
        code, carrier, phase, modulation, mod_phase, depth, duty, seed, shape, enabled, pm = self._get_settings_words(channel)
        if code:
            raise RuntimeError(self.get_error_message(code))
        with localcontext() as context:
            context.prec = 80
            turn = Decimal(1 << self._info[channel][0])
            return dict(channel=channel, carrier_hz=Decimal(carrier) * self.sample_rate / turn,
                        carrier_phase=Decimal(phase) * 360 / turn,
                        modulation_hz=Decimal(modulation) * self.sample_rate / turn,
                        modulation_phase=Decimal(mod_phase) * 360 / turn,
                        deviation=Decimal(depth) * 360 / turn, duty=Decimal(duty) / turn,
                        seed=seed, waveform=Waveform(shape).name.lower(),
                        output_enabled=enabled, pm_enabled=pm)

    def _validate_channel(self, channel):
        if isinstance(channel, bool) or not isinstance(channel, Integral) or not 0 <= channel < self.channels:
            raise ValueError(f"Channel must be an integer between 0 and {self.channels - 1}")

    def available_waveforms(self, channel=0):
        self._validate_channel(channel)
        return tuple(shape for shape in Waveform if self._info[channel][5] & (1 << shape))

    def info(self, channel=0):
        self._validate_channel(channel)
        fields = ("phase_width", "modulation_width", "lut_bits", "prbs_width", "output_width", "capabilities")
        return dict(zip(fields, self._info[channel]), sample_rate=self.sample_rate,
                    waveforms=tuple(shape.name.lower() for shape in self.available_waveforms(channel)))

    def _check_response(self, message):
        if message:
            raise RuntimeError(message)
        return self

    @command()
    def get_channel_count(self):
        return self.client.recv_uint32()

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

    @command()
    def configure_words_checked(self, channel, carrier_increment, carrier_phase,
                                modulation_increment, modulation_phase, deviation, duty,
                                seed, waveform, output_enabled, pm_enabled,
                                restart_carrier, restart_modulation):
        return self.client.recv_string()

    @command(funcname="mute")
    def _mute(self, channel):
        return self.client.recv_string()

    @command(funcname="restart")
    def _restart(self, channel):
        return self.client.recv_string()

    @command(funcname="set_output_enabled")
    def _set_output_enabled(self, channel, enabled):
        return self.client.recv_string()

    @command(funcname="set_pm_enabled")
    def _set_pm_enabled(self, channel, enabled):
        return self.client.recv_string()

    @command(funcname="set_carrier_increment")
    def _set_carrier_increment(self, channel, word):
        return self.client.recv_string()

    @command(funcname="set_modulation_increment")
    def _set_modulation_increment(self, channel, word):
        return self.client.recv_string()

    @command(funcname="set_phase_word")
    def _set_phase_word(self, channel, word):
        return self.client.recv_string()

    @command(funcname="set_deviation_word")
    def _set_deviation_word(self, channel, word):
        return self.client.recv_string()

    def mute(self, channel=0):
        self._validate_channel(channel)
        return self._check_response(self._mute(channel))

    def restart(self, channel=0):
        self._validate_channel(channel)
        return self._check_response(self._restart(channel))

    def enable_output(self, enabled=True, channel=0):
        self._validate_channel(channel)
        return self._check_response(self._set_output_enabled(channel, enabled))

    def enable_pm(self, enabled=True, channel=0):
        self._validate_channel(channel)
        return self._check_response(self._set_pm_enabled(channel, enabled))

    def set_frequency(self, hz, channel=0):
        self._validate_channel(channel)
        word = frequency_word(hz, self.sample_rate, self._info[channel][0])
        return self._check_response(self._set_carrier_increment(channel, word))

    def set_modulation_frequency(self, hz, channel=0):
        self._validate_channel(channel)
        word = frequency_word(hz, self.sample_rate, self._info[channel][0])
        return self._check_response(self._set_modulation_increment(channel, word))

    def set_phase(self, degrees, channel=0):
        self._validate_channel(channel)
        word = phase_word(degrees, self._info[channel][0])
        return self._check_response(self._set_phase_word(channel, word))

    def set_deviation(self, degrees, channel=0):
        self._validate_channel(channel)
        word = phase_word(degrees, self._info[channel][0], deviation=True)
        return self._check_response(self._set_deviation_word(channel, word))

    def tone(self, carrier_hz=10_000_000, channel=0, phase=0, restart=True):
        return self.configure(channel=channel, carrier_hz=carrier_hz, carrier_phase=phase,
                              pm_enabled=False, restart=restart)

    def configure(self, channel=0, carrier_hz=10_000_000, carrier_phase=0,
                  waveform=Waveform.SINE, modulation_hz=1_000, modulation_phase=0,
                  deviation=30, duty="0.5", seed=1, output_enabled=True,
                  pm_enabled=True, restart=True):
        self._validate_channel(channel)
        width = self._info[channel][0]
        if not 32 <= width <= 48:
            raise ValueError("Unsupported phase width")
        if isinstance(waveform, str):
            try:
                waveform = Waveform[waveform.strip().upper().replace("-", "_").replace(" ", "_")]
            except KeyError:
                raise ValueError(f"Unknown waveform {waveform!r}; use a Waveform name") from None
        waveform = Waveform(waveform)
        if pm_enabled and waveform not in self.available_waveforms(channel):
            choices = ", ".join(shape.name.lower() for shape in self.available_waveforms(channel)) or "none"
            raise ValueError(f"Waveform {waveform.name.lower()} is unavailable; available waveforms: {choices}")
        with localcontext() as context:
            context.prec = 80
            duty_value = _number(duty, "Duty")
            if not 0 <= duty_value <= 1:
                raise ValueError("Duty must be between 0 and 1")
            duty_word = int((duty_value * (1 << width)).to_integral_value(rounding=ROUND_HALF_UP))
        if isinstance(seed, bool) or not isinstance(seed, Integral) or not 0 < seed < 1 << 32:
            raise ValueError("Seed must be a nonzero 32-bit integer")
        if waveform == Waveform.PRBS and not seed & ((1 << self._info[channel][3]) - 1):
            raise ValueError(f"PRBS seed must be nonzero within the PN{self._info[channel][3]} order")
        message = self.configure_words_checked(
            channel, frequency_word(carrier_hz, self.sample_rate, width),
            phase_word(carrier_phase, width),
            frequency_word(modulation_hz, self.sample_rate, width),
            phase_word(modulation_phase, width), phase_word(deviation, width, True),
            duty_word, seed, int(waveform), output_enabled, pm_enabled, restart, restart)
        return self._check_response(message)


def main(argv=None):
    import argparse
    from koheron import connect
    def number(text):
        try:
            return _number(text, "Value")
        except ValueError as error:
            raise argparse.ArgumentTypeError(str(error)) from None
    parser = argparse.ArgumentParser(description="Generate or control an ALPHA250 phase-modulated carrier")
    parser.add_argument("host")
    parser.add_argument("--channel", type=int, default=0)
    parser.add_argument("--carrier-hz", type=number, default=Decimal("10000000"))
    parser.add_argument("--phase", type=number, default=Decimal(0), help="Carrier phase in degrees")
    parser.add_argument("--modulation-hz", type=number, default=Decimal(1000))
    parser.add_argument("--waveform", choices=[shape.name.lower() for shape in Waveform], default="sine")
    parser.add_argument("--deviation", type=number, default=Decimal(30), help="Phase deviation in degrees")
    parser.add_argument("--duty", type=number, default=Decimal("0.5"))
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--no-restart", action="store_true", help="Preserve oscillator phase")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--tone", action="store_true", help="Generate an unmodulated tone")
    modes.add_argument("--mute", action="store_true", help="Mute without changing the current settings")
    modes.add_argument("--status", action="store_true", help="Read current settings without changing the output")
    modes.add_argument("--info", action="store_true", help="Print channel capabilities without changing the output")
    args = parser.parse_args(argv)
    try:
        generator = PhaseModulator(connect(args.host, name="phase-modulator"))
        if args.info:
            print(generator.info(args.channel))
        elif args.status:
            print(generator.settings(args.channel))
        elif args.mute:
            generator.mute(args.channel)
            print(f"OUT{args.channel} muted")
        else:
            generator.configure(channel=args.channel, carrier_hz=args.carrier_hz,
                                carrier_phase=args.phase, modulation_hz=args.modulation_hz,
                                waveform=args.waveform, deviation=args.deviation, duty=args.duty,
                                seed=args.seed, pm_enabled=not args.tone, restart=not args.no_restart)
            mode = "tone" if args.tone else f"{args.waveform} PM at {args.modulation_hz} Hz, deviation {args.deviation} degrees"
            print(f"OUT{args.channel}: {args.carrier_hz} Hz carrier, {mode}")
    except (ValueError, RuntimeError, OSError) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    main()
