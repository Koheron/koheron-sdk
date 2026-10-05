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
    generator.get_sample_rate = lambda: 250_000_000
    generator.channels = 2
    generator._info = ((48, 24, 14, 31, 16, 1023),) * 2
    calls = []
    generator.configure_words_checked = lambda *args: calls.append(args) or ""
    generator.configure(waveform=module.Waveform.PULSE, duty=1, deviation=360)
    words = calls[-1]
    assert words[5] == words[6] == 1 << 48
    assert words[8] == module.Waveform.PULSE
    with pytest.raises(ValueError):
        generator.configure(duty="NaN")
    generator._info = ((48, 24, 14, 31, 16, 2),) * 2
    with pytest.raises(ValueError):
        generator.configure(waveform=module.Waveform.SINE)
    generator.configure(pm_enabled=False)  # Tone on a reduced build.


def test_single_channel_rejects_absent_output_before_rpc():
    generator = object.__new__(module.PhaseModulator)
    generator.channels = 1
    generator.get_phase_width = lambda channel: pytest.fail("Unexpected RPC")
    with pytest.raises(ValueError, match="between 0 and 0"):
        generator.configure(channel=1)


@pytest.fixture
def generator():
    pm = object.__new__(module.PhaseModulator)
    pm.channels = 1
    pm.sample_rate = 250_000_000
    pm.get_sample_rate = lambda: 250_000_000
    pm._info = ((48, 24, 14, 7, 16, 1023),)
    return pm


def test_frequency_updates_follow_a_changed_host_clock(generator):
    calls = []
    generator.get_sample_rate = lambda: 200_000_000
    generator._set_carrier_increment = lambda *args: calls.append(args) or ""
    generator.set_frequency(50_000_000)
    assert calls == [(0, 1 << 46)]
    assert generator.info()["sample_rate"] == 200_000_000
    with pytest.raises(ValueError, match="below Nyquist"):
        generator.set_frequency(100_000_000)
    assert len(calls) == 1


def test_connection_discovers_metadata_once(monkeypatch):
    calls = []
    monkeypatch.setattr(module.PhaseModulator, "get_sample_rate", lambda self: 250_000_000)
    monkeypatch.setattr(module.PhaseModulator, "get_channel_count", lambda self: 2)
    monkeypatch.setattr(module.PhaseModulator, "get_channel_info", lambda self, channel: calls.append(channel) or (48, 24, 14, 31, 16, 1023))
    monkeypatch.setattr(module.PhaseModulator, "configure_words_checked", lambda *args: "")
    pm = module.PhaseModulator(object())
    assert calls == [0, 1]
    assert pm.configure(waveform="sine") is pm
    pm.configure(waveform="BPSK", channel=1)
    assert calls == [0, 1]
    assert pm.info()["sample_rate"] == 250_000_000
    assert "bpsk" in pm.info()["waveforms"]


def test_initialization_error_reaches_client(monkeypatch):
    monkeypatch.setattr(module.PhaseModulator, "get_sample_rate", lambda self: 250_000_000)
    monkeypatch.setattr(module.PhaseModulator, "get_channel_count", lambda self: 0)
    monkeypatch.setattr(module.PhaseModulator, "get_initialization_error", lambda self: "Unsupported DDS PM metadata")
    with pytest.raises(RuntimeError, match="Unsupported DDS PM metadata"):
        module.PhaseModulator(object())


def test_waveform_names_and_backend_errors(generator):
    calls = []
    generator.configure_words_checked = lambda *args: calls.append(args) or ""
    assert generator.configure(waveform="up ramp") is generator
    assert calls[-1][8] == module.Waveform.UP_RAMP
    with pytest.raises(ValueError, match="Unknown waveform"):
        generator.configure(waveform="unknown")
    generator.configure_words_checked = lambda *args: "DDS PM commit acknowledgement timed out; settings may still apply"
    with pytest.raises(RuntimeError, match="settings may still apply"):
        generator.configure()


def test_native_precision_for_partial_updates(generator):
    calls = []
    generator._set_phase_word = lambda *args: calls.append(("phase", args)) or ""
    generator._set_deviation_word = lambda *args: calls.append(("deviation", args)) or ""
    generator._set_carrier_increment = lambda *args: calls.append(("frequency", args)) or ""
    with localcontext() as context:
        context.prec = 80
        step = Decimal(360) / (1 << 48)
        generator.set_phase(-step)
        generator.set_deviation(step)
    generator.set_frequency("0.000001")
    assert calls == [("phase", (0, (1 << 48) - 1)), ("deviation", (0, 1)), ("frequency", (0, 1))]


def test_mute_restart_and_tone(generator):
    calls = []
    generator._mute = lambda channel: calls.append(("mute", channel)) or ""
    generator._restart = lambda channel: calls.append(("restart", channel)) or ""
    generator._set_output_enabled = lambda *args: calls.append(("enable", args)) or ""
    generator.configure_words_checked = lambda *args: calls.append(("configure", args)) or ""
    assert generator.mute().restart().enable_output() is generator
    assert calls[:3] == [("mute", 0), ("restart", 0), ("enable", (0, True))]
    generator.tone(5_000_000, phase=90)
    words = calls[-1][1]
    assert words[2] == 1 << 46 and words[10] is False
    generator._mute = lambda channel: "Previous DDS PM commit is still pending"
    with pytest.raises(RuntimeError, match="still pending"):
        generator.mute()


@pytest.mark.parametrize("channel", [-1, 1, 1.0, True])
def test_invalid_channels_do_not_issue_commands(generator, channel):
    generator._mute = lambda *args: pytest.fail("Unexpected RPC")
    with pytest.raises(ValueError, match="Channel must be an integer"):
        generator.mute(channel)


@pytest.mark.parametrize("seed", [0, 1 << 32, 1.5, True])
def test_invalid_seed_does_not_issue_configuration(generator, seed):
    generator.configure_words_checked = lambda *args: pytest.fail("Unexpected RPC")
    with pytest.raises(ValueError, match="32-bit integer"):
        generator.configure(seed=seed)


def test_prbs_order_seed_validation(generator):
    generator.configure_words_checked = lambda *args: pytest.fail("Unexpected RPC")
    with pytest.raises(ValueError, match="PN7"):
        generator.configure(waveform="prbs", seed=128)


@pytest.mark.parametrize("text", ["abc", "NaN", "Infinity"])
def test_cli_invalid_numbers_fail_before_connect(monkeypatch, capsys, text):
    import koheron
    monkeypatch.setattr(koheron, "connect", lambda *args, **kwargs: pytest.fail("Unexpected connection"))
    with pytest.raises(SystemExit) as error:
        module.main(["example", "--carrier-hz", text])
    assert error.value.code == 2
    assert "finite" in capsys.readouterr().err


def test_cli_mute_uses_existing_settings(monkeypatch, capsys):
    import koheron
    calls = []
    class FakeGenerator:
        def __init__(self, client):
            pass
        def mute(self, channel):
            calls.append(channel)
    monkeypatch.setattr(koheron, "connect", lambda *args, **kwargs: object())
    monkeypatch.setattr(module, "PhaseModulator", FakeGenerator)
    module.main(["example", "--mute", "--channel", "1"])
    assert calls == [1]
    assert "OUT1 muted" in capsys.readouterr().out


def test_settings_roundtrip_preserves_native_precision(generator):
    native = (0, 0x123456789abc, 1, 17, (1 << 48)-1, 1 << 48, 1 << 47, 1, 2, True, True)
    generator._get_settings_words = lambda channel: native
    snapshot = generator.settings()
    calls = []
    generator.configure_words_checked = lambda *args: calls.append(args) or ""
    generator.configure(**snapshot)
    assert calls[-1][1:11] == native[1:]
    generator._get_settings_words = lambda channel: (12,) + native[1:]
    generator.get_error_message = lambda code: "Previous DDS PM commit is still pending"
    with pytest.raises(RuntimeError, match="still pending"):
        generator.settings()
