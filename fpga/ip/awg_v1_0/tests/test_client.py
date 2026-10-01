import importlib.util
from pathlib import Path
from decimal import Decimal, localcontext

import pytest

path = Path(__file__).resolve().parents[4] / "examples/alpha250/phase-modulator/phase_modulator.py"
spec = importlib.util.spec_from_file_location("phase_modulator", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def test_native_phase_precision_and_wrap():
    with localcontext() as context:
        context.prec = 80
        step = Decimal(360) / (1 << 48)
        assert module.phase_word(step) == 1
        assert module.phase_word(-step) == (1 << 48) - 1
        assert module.phase_word(step * 17) == 17
    assert module.phase_word(360) == 0
    assert module.phase_word(360, deviation=True) == 1 << 48
    assert module.phase_word(90) == 1 << 46


def test_frequency_words():
    assert module.frequency_word(62_500_000, 250_000_000) == 1 << 46
    assert module.frequency_word(0, 250_000_000) == 0
    assert module.frequency_word("0.000001", 250_000_000) == 1


@pytest.mark.parametrize("value", ["NaN", "Infinity", "-Infinity"])
def test_nonfinite_inputs(value):
    with pytest.raises(ValueError):
        module.phase_word(value)
    with pytest.raises(ValueError):
        module.frequency_word(value, 250_000_000)


def test_invalid_ranges():
    for value in [-1, 361]:
        with pytest.raises(ValueError):
            module.phase_word(value, deviation=True)
    for value in [-1, 125_000_000]:
        with pytest.raises(ValueError):
            module.frequency_word(value, 250_000_000)


def test_configuration_keeps_native_words():
    generator = object.__new__(module.PhaseModulator)
    generator.sample_rate = 250_000_000
    generator.get_phase_width = lambda channel: 48
    generator.get_capabilities = lambda channel: 1023
    calls = []
    generator.configure_words = lambda *args: calls.append(args) or True
    generator.configure(waveform=module.Waveform.PULSE, duty=1, deviation=360)
    words = calls[-1]
    assert words[5] == words[6] == 1 << 48
    assert words[8] == module.Waveform.PULSE
    with pytest.raises(ValueError):
        generator.configure(duty="NaN")
    generator.get_capabilities = lambda channel: 2
    with pytest.raises(ValueError):
        generator.configure(waveform=module.Waveform.SINE)
    generator.configure(pm_enabled=False)  # Tone on a reduced build.
