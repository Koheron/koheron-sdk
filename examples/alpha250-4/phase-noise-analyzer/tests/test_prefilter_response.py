"""Ideal mixer/phase model; RTL arithmetic is checked separately by xsim."""
import numpy as np
from scipy import signal

fs = 200e6
taps = np.ones(16)
for _ in range(3):
    taps = np.convolve(taps, np.ones(16))
taps /= 65536
assert len(taps) == 61 and sum(taps) == 1
assert np.array_equal(taps, taps[::-1])

def response(frequency):
    return abs(np.sum(taps * np.exp(-2j * np.pi * frequency / fs * np.arange(61))))

assert 20 * np.log10(response(20e6)) < -57
assert 20 * np.log10(response(0.5e6)) > -0.1
time = np.arange(400000) / fs
for frequency in (1e5, 5e5, 1e6):
    phase = .001 * np.sin(2 * np.pi * frequency * time)
    adc = np.cos(2 * np.pi * 10e6 * time + phase)
    mixed = adc * np.exp(-2j * np.pi * 10e6 * time)
    measured_phase = np.angle(signal.lfilter(taps, [1], mixed)[200:])
    basis = np.column_stack((np.sin(2 * np.pi * frequency * time[200:]),
                             np.cos(2 * np.pi * frequency * time[200:]),
                             np.ones(len(time) - 200)))
    fit = np.linalg.lstsq(basis, measured_phase, rcond=None)[0]
    gain = np.hypot(*fit[:2]) / .001
    assert abs(gain - response(frequency)) < 1e-4
    print(f"Phase modulation at {frequency:g} Hz: gain {gain:.8f}")
print("Filter response and ideal phase-modulation checks passed")
