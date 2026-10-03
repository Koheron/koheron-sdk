"""Validate known sine PM using DAC0 -> ADC0 (LV input range)."""
import os
from pathlib import Path
import sys
import time

import numpy as np
from koheron import connect
from phase_noise_analyzer import PhaseNoiseAnalyzer

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "alpha250/phase-modulator"))
from phase_modulator import PhaseModulator

host = os.getenv("HOST", "192.168.1.84")
client = connect(host, "phase-noise-analyzer")
analyzer = PhaseNoiseAnalyzer(client)
generator = PhaseModulator(client)
original = analyzer.get_parameters()
tracking = analyzer.get_tracking_parameters()
precision = analyzer.get_precision_status()[0]
generator_words = [generator._get_settings_words(channel) for channel in range(2)]
bits = int(os.getenv("PHASE_BITS", "0"))
rate = int(os.getenv("CIC_RATE", "20"))
beta = float(os.getenv("PM_RADIANS", ".1"))
assert beta > 0 and np.isfinite(beta)


def wait_captures(count):
    first = analyzer.get_precision_status()[4]
    deadline = time.monotonic() + max(10, (count + 6) * 3 * 262144 / fs)
    while time.monotonic() < deadline:
        status = analyzer.get_precision_status()
        assert status[3] != 2, "Phase overrange: reduce precision or LO offset"
        if status[4] >= first + count and status[3] == 1:
            assert status[:2] == (bits, bits)
            return
        time.sleep(.02)
    raise TimeoutError("Analyzer did not publish enough valid captures")


try:
    analyzer.set_tracking_enabled(False)
    analyzer.set_channel(0)
    analyzer.set_cic_rate(rate)
    analyzer.set_fft_navg(8)
    analyzer.set_local_oscillator(0, 10e6)
    assert analyzer.set_phase_precision(bits), "Unsupported precision"
    parameters = analyzer.get_parameters()
    fs, bins = parameters[1], parameters[0]
    assert bins == 16385 and fs == 125e6 / (2 * rate)
    df = fs / (2 * (bins - 1))
    tone_bin = 64
    tone_hz = tone_bin * df
    generator.configure(carrier_hz=10e6, modulation_hz=tone_hz,
                        deviation=beta * 180 / np.pi, waveform="sine",
                        output_enabled=True, pm_enabled=False, channel=0)
    wait_captures(12)
    baseline = analyzer.get_phase_noise()
    generator.enable_pm(True, channel=0)
    wait_captures(12)
    pm = analyzer.get_phase_noise()
    tone_power = np.sum((pm - baseline)[tone_bin-2:tone_bin+3]) * df
    expected = beta**2 / 2
    relative_error = tone_power / expected - 1
    print(f"CIC {rate}, +{bits} bits, PM {beta:g} rad at {tone_hz:.6f} Hz")
    print(f"Phase power: {tone_power:.6g} rad²; expected {expected:.6g}; error {relative_error:+.2%}")
    folder = Path("tmp/tests/red-pitaya-phase-noise-analyzer")
    folder.mkdir(parents=True, exist_ok=True)
    np.savez(folder / f"loopback-r{rate}-b{bits}-pm{beta:g}.npz",
             frequency=np.arange(bins)*df, baseline=baseline, pm=pm,
             expected_power=expected, measured_power=tone_power)
    assert np.isfinite(pm).all() and abs(relative_error) < .05
finally:
    analyzer.set_tracking_enabled(False)
    analyzer.set_cic_rate(original[3])
    analyzer.set_channel(original[2])
    analyzer.set_fft_navg(original[4])
    analyzer.set_local_oscillator(0, tracking[5])
    analyzer.set_local_oscillator(1, tracking[6])
    analyzer.set_phase_precision(precision)
    for channel, words in enumerate(generator_words):
        assert not generator.configure_words_checked(channel, *words[1:], True, True)
        assert generator._get_settings_words(channel) == words
    analyzer.set_tracking_enabled(tracking[0])
    client.sock.close()
