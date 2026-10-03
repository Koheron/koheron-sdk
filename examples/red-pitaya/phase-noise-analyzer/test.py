"""Validate known sine PM using DAC0 -> ADC0 (LV input range)."""
import os
from pathlib import Path
import sys
import time

import numpy as np
from koheron import connect
from phase_noise_analyzer import PhaseNoiseAnalyzer

# Use the existing board-independent PM client; it reads the actual clock.
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "alpha250/phase-modulator"))
from phase_modulator import PhaseModulator

host = os.getenv("HOST", "192.168.1.84")
client = connect(host, "phase-noise-analyzer")
analyzer = PhaseNoiseAnalyzer(client)
generator = PhaseModulator(client)
analyzer.set_tracking_enabled(False)
analyzer.set_channel(0)
analyzer.set_cic_rate(20)
analyzer.set_fft_navg(8)
analyzer.set_local_oscillator(0, 10e6)
parameters = analyzer.get_parameters()
fs, bins = parameters[1], parameters[0]
assert bins == 16385 and fs == 125e6 / 40
fft_size = 2 * (bins - 1)
df = fs / fft_size
tone_bin, beta = 64, .1
tone_hz = tone_bin * df
generator.configure(carrier_hz=10e6, modulation_hz=tone_hz,
                    deviation=beta * 180 / np.pi, waveform="sine",
                    output_enabled=True, pm_enabled=False, channel=0)
settle = 14 * 262144 / fs

try:
    time.sleep(settle)
    baseline = analyzer.get_phase_noise()
    generator.enable_pm(True, channel=0)
    time.sleep(settle)
    pm = analyzer.get_phase_noise()
    tone_power = np.sum((pm - baseline)[tone_bin-2:tone_bin+3]) * df
    expected = beta**2 / 2
    relative_error = tone_power / expected - 1
    print(f"PM tone: {tone_hz:.6f} Hz; integrated phase power: {tone_power:.6g} rad²")
    print(f"Expected: {expected:.6g} rad²; error: {relative_error:+.2%}")
    folder = Path("tmp/tests/red-pitaya-phase-noise-analyzer")
    folder.mkdir(parents=True, exist_ok=True)
    np.savez(folder / "loopback.npz", frequency=np.arange(bins)*df,
             baseline=baseline, pm=pm, expected_power=expected, measured_power=tone_power)
    assert np.isfinite(pm).all() and abs(relative_error) < .05
finally:
    # Leave an unmodulated carrier for the live analyzer display.
    generator.enable_pm(False, channel=0)
    client.sock.close()
