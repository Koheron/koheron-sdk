"""Logarithmic chirp settings, integer phase reference, and transfer estimate."""
from dataclasses import dataclass
from decimal import Decimal, localcontext, ROUND_HALF_UP

import numpy as np
from scipy.fft import rfft

FS = 250_000_000
CAPTURE_SAMPLES = 64 * 1024 * 1024
TAPER_SAMPLES = 1 << 18
PHASE_MASK = (1 << 48) - 1
DDS_AMPLITUDE = 32766  # DDS Compiler Full_Range, 16-bit output.


@dataclass(frozen=True)
class Chirp:
    start_hz: float = 500.0
    stop_hz: float = 110_000_000.0
    duration: float = 0.250

    def __post_init__(self):
        if not (0 < self.start_hz < self.stop_hz < FS / 2):
            raise ValueError("Require 0 < start < stop < 125 MHz")
        samples = round(self.duration * FS)
        if not (2 * TAPER_SAMPLES < samples <= CAPTURE_SAMPLES - 1024):
            raise ValueError("Sweep must fit inside the capture, including its tapers")
        with localcontext() as ctx:
            ctx.prec = 80
            start, stop = Decimal(str(self.start_hz)), Decimal(str(self.stop_hz))
            step = (stop / start).ln() / (samples - 1)
            scale = Decimal(2) ** 64
            def nearest(value):
                return int(value.to_integral_value(rounding=ROUND_HALF_UP))
            coefficient = nearest(((16 * step).exp() - 1) * scale)
            seeds = tuple(nearest(start / FS * scale * (j * step).exp())
                          for j in range(16))
        if not (0 < coefficient < 1 << 48):
            raise ValueError("Sweep is too fast for the fixed-point coefficient")
        object.__setattr__(self, "samples", samples)
        object.__setattr__(self, "coefficient", coefficient)
        object.__setattr__(self, "seeds", seeds)


def phase_blocks(chirp, block_size=65536):
    """Exact FPGA integer recurrence. Blocks limit host temporary memory."""
    state = list(chirp.seeds)
    phase = 0
    for first in range(0, chirp.samples, block_size):
        size = min(block_size, chirp.samples - first)
        increments = np.empty(size, dtype=np.uint64)
        for k in range(size):
            lane = (first + k) & 15
            value = state[lane]
            increments[k] = (value + (1 << 15)) >> 16
            state[lane] = value + ((value * chirp.coefficient + (1 << 63)) >> 64)
        # uint64 wrap is harmless: 2^48 divides 2^64.
        phases = (np.cumsum(increments, dtype=np.uint64) + np.uint64(phase)) & np.uint64(PHASE_MASK)
        phase = int(phases[-1])
        yield first, phases


def reference(chirp, length=CAPTURE_SAMPLES):
    """Normalized DAC reference at the capture start, followed by silence.

    Phase and taper arithmetic match RTL. The 16-bit Full_Range sine mapping
    is checked exhaustively against the DDS IP by run_dds_sim.tcl.
    """
    if length < chirp.samples:
        raise ValueError("Reference is shorter than the excitation")
    result = np.zeros(length, dtype=np.float32)
    for first, phase in phase_blocks(chirp):
        n = np.arange(first, first + len(phase), dtype=np.int64)
        gain = np.minimum(np.minimum(n, chirp.samples - 1 - n), TAPER_SAMPLES)
        angle = (phase >> np.uint64(32)).astype(np.float64) * (2 * np.pi / (1 << 16))
        sine = np.rint(DDS_AMPLITUDE * np.sin(angle)).astype(np.int64)
        result[first:first + len(phase)] = ((sine * gain) >> 18).astype(np.float32) / 32768
    return result


def transfer_response(adc, excitation, low_hz=1000.0, high_hz=100_000_000.0,
                      points_per_decade=100):
    """Whole-record FFT ratio, sampled on a logarithmic grid.

    No Welch segmentation or averaging across rotating phase: the chirp and
    its complete response are processed together. Resolution is fs/N.
    """
    if len(adc) != len(excitation):
        raise ValueError("ADC and reference lengths differ")
    if not (0 < low_hz < high_hz < FS / 2) or points_per_decade < 1:
        raise ValueError("Invalid response frequency grid")
    n = len(adc)
    count = int(np.ceil(np.log10(high_hz / low_hz) * points_per_decade)) + 1
    bins = np.unique(np.rint(np.geomspace(low_hz, high_hz, count) * n / FS).astype(np.int64))
    if bins[0] < 1 or bins[-1] >= n // 2:
        raise ValueError("Record is too short for this frequency grid")
    spectrum = rfft(excitation, workers=2)
    x = spectrum[bins].copy()
    del spectrum
    spectrum = rfft(adc.astype(np.float32) / 32768, workers=2)
    y = spectrum[bins].copy()
    del spectrum
    if np.any(np.abs(x) < np.max(np.abs(x)) * 1e-6):
        raise ValueError("Insufficient excitation at requested frequencies")
    return bins * FS / n, y / x
