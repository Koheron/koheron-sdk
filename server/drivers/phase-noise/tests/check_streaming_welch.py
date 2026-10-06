"""Independent SciPy oracle for each new segment and rolling Welch output."""
import sys
import numpy as np
from scipy import signal

size, hops, fs = 512, 12, 123456
raw = np.fromfile(sys.argv[1] + ".raw", dtype="<i4").reshape(2, -1)
actual = np.fromfile(sys.argv[1], dtype="<f4").reshape(hops, size // 2 + 1, 5)
window = signal.windows.hann(size, sym=True)
expected = np.empty_like(actual, dtype=np.float64)
for hop in range(hops):
    offset = hop * size // 2
    # Remove a linear fit independently from each segment. Center raw counts
    # in float64 before applying the very different per-channel calibrations.
    x = signal.detrend(raw[0, offset:offset + size].astype(np.float64)) * 1e-5
    y = signal.detrend(raw[1, offset:offset + size].astype(np.float64)) * 1e-11
    _, power = signal.periodogram(x, fs, window=window, detrend=False)
    _, cross = signal.csd(x, y, fs, window=window, nperseg=size, noverlap=0, detrend=False)
    expected[hop, :, 0] = power
    expected[hop, :, 2] = cross.real
    expected[hop, :, 3] = cross.imag
    first = max(0, hop - 2)
    expected[hop, :, 1] = expected[first:hop + 1, :, 0].mean(axis=0)
    expected[hop, :, 4] = expected[first:hop + 1, :, 2].mean(axis=0)

errors = np.sqrt(np.sum((actual - expected) ** 2, axis=(0, 1)) /
                 np.sum(expected ** 2, axis=(0, 1)))
assert np.all(errors < 2e-6), errors
assert np.all(actual[:, 19, 2] < 0)
print("SciPy streaming auto/CSD, rolling three-segment averages, large drift and 120 dB channel ratio: PASS", errors)

edge_size = 32768
edge_raw = np.fromfile(sys.argv[1] + ".edge.raw", dtype="<i4").reshape(4, 2, edge_size)
edge_actual = np.fromfile(sys.argv[1] + ".edge", dtype="<f4").reshape(4, edge_size // 2 + 1, 3)
window = signal.windows.hann(edge_size, sym=True)
for index, pair in enumerate(edge_raw):
    centered = pair.astype(np.float64) - pair[:, :1].astype(np.float64)
    x = signal.detrend(centered[0]) * 1e-5
    y = signal.detrend(centered[1]) * 1e-11
    _, power = signal.periodogram(x, fs, window=window, detrend=False)
    _, cross = signal.csd(x, y, fs, window=window, nperseg=edge_size, noverlap=0, detrend=False)
    expected = np.column_stack((power, cross.real, cross.imag))
    # Compare complex CSD together: near-zero quadrature has no relative scale.
    auto_error = np.linalg.norm(edge_actual[index, :, 0] - power) / np.linalg.norm(power)
    cross_error = np.linalg.norm(edge_actual[index, :, 1:] - expected[:, 1:]) / np.linalg.norm(expected[:, 1:])
    assert max(auto_error, cross_error) < 2e-6, (index, auto_error, cross_error)
    print("SciPy full-size offset/drift/endpoint case", index, "PASS", auto_error, cross_error)
