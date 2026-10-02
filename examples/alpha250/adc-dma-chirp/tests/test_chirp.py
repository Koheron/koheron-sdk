import sys
from pathlib import Path
from types import SimpleNamespace

import numpy as np
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from chirp import Chirp, FS, PHASE_MASK, phase_blocks, transfer_response


def test_phase_is_independent_of_host_block_size():
    chirp = Chirp()
    small = SimpleNamespace(seeds=chirp.seeds, coefficient=chirp.coefficient, samples=10000)
    a = np.concatenate([p for _, p in phase_blocks(small, 127)])
    b = np.concatenate([p for _, p in phase_blocks(small, 1024)])
    np.testing.assert_array_equal(a, b)
    phase = 0
    states = list(chirp.seeds)
    for n, actual in enumerate(a):
        lane = n % 16
        value = states[lane]
        phase = (phase + ((value + 32768) // 65536)) & PHASE_MASK
        states[lane] += (value * chirp.coefficient + (1 << 63)) // (1 << 64)
        assert int(actual) == phase


def test_known_gain_delay_and_filter():
    rng = np.random.default_rng(42)
    x = rng.standard_normal(65536).astype(np.float32) * 0.1
    x[-128:] = 0  # Capture the complete FIR response.
    taps = np.r_[np.zeros(7), [0.2, 0.4, 0.2]]
    y = np.convolve(x, taps)[:len(x)] * 32768
    f, h = transfer_response(y, x, low_hz=1e5, high_hz=100e6)
    expected = sum(tap * np.exp(-2j * np.pi * f / FS * k)
                   for k, tap in enumerate(taps))
    np.testing.assert_allclose(h, expected, atol=2e-5, rtol=2e-5)


@pytest.mark.parametrize("args", [dict(start_hz=0), dict(stop_hz=125e6),
                                      dict(duration=0.3), dict(duration=0.0001)])
def test_invalid_settings(args):
    with pytest.raises(ValueError):
        Chirp(**args)
