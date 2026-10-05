import importlib.util
from pathlib import Path

import numpy as np
import pytest

MODULE = Path(__file__).resolve().parents[1] / "adc_dma.py"
SPEC = importlib.util.spec_from_file_location("quad_adc_dma", MODULE)
adc_dma = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(adc_dma)


class FakeDma(adc_dma.AdcDma):
    def __init__(self, status=1, corrupt=None, truncated=False):
        self.status = status
        self.corrupt = corrupt
        self.truncated = truncated
        self.stops = 0
        self.blocks = []

    def stop(self):
        self.stops += 1

    def set_sampling_frequency(self, selection):
        assert selection == 0

    def get_adc_sampling_freq(self):
        return np.array([200e6, 200e6])

    def configure(self, samples, pattern):
        self.samples = samples
        return True

    def start(self):
        return True

    def get_status(self):
        return self.status

    def get_diagnostics(self):
        return [0] * 6

    def get_adc_block(self, pair, index):
        self.blocks.append((pair, index))
        first = index * adc_dma.PACKET_SAMPLES
        frames = min(adc_dma.PACKET_SAMPLES, self.samples - first)
        indices = np.arange(first, first + frames, dtype=np.uint32)
        data = np.empty((frames, 2), dtype="<u2")
        for channel in range(2):
            codes = (indices if channel == 0 else indices >> 16).astype(np.uint16)
            data[:, channel] = codes ^ np.uint16((2 * pair + channel) * 0x4000)
        if self.corrupt == (pair, index):
            data[-1, -1] ^= 1
        words = data.reshape(-1).view("<u4")
        return words[:-1] if self.truncated else words


@pytest.fixture(autouse=True)
def no_settle_delay(monkeypatch):
    monkeypatch.setattr(adc_dma.time, "sleep", lambda _: None)


def test_short_final_block_channel_order_and_memmap(tmp_path):
    samples = adc_dma.PACKET_SAMPLES + 6
    output = np.lib.format.open_memmap(tmp_path / "record.npy", mode="w+", dtype="<i2", shape=(samples, 4))
    driver = FakeDma()
    assert driver.acquire(samples, test_pattern=True, output=output) is output
    assert driver.blocks == [(0, 0), (1, 0), (0, 1), (1, 1)]
    indices = np.arange(samples, dtype=np.uint32)
    for channel in range(4):
        expected = (indices if channel % 2 == 0 else indices >> 16).astype(np.uint16)
        np.testing.assert_array_equal(output[:, channel].view(np.uint16), expected ^ np.uint16(channel * 0x4000))
    assert driver.stops == 2


def test_corrupted_last_sample_rejected():
    driver = FakeDma(corrupt=(1, 1))
    with pytest.raises(RuntimeError, match="channel 3, sample 65541"):
        driver.acquire(adc_dma.PACKET_SAMPLES + 6, test_pattern=True)
    assert driver.stops == 2


@pytest.mark.parametrize("status", [2, 3, 4, 5, 6])
def test_hardware_failure_rejected_without_download(status):
    driver = FakeDma(status=status)
    with pytest.raises(RuntimeError, match="diagnostics"):
        driver.acquire(2)
    assert driver.blocks == []
    assert driver.stops == 2


def test_incomplete_download_rejected():
    driver = FakeDma(truncated=True)
    with pytest.raises(RuntimeError, match="Incomplete ADC block"):
        driver.acquire(2)
    assert driver.stops == 2


def test_timeout_stops_both_dmas(monkeypatch):
    times = iter([0.0, 2.0])
    monkeypatch.setattr(adc_dma.time, "monotonic", lambda: next(times))
    driver = FakeDma(status=0)
    with pytest.raises(TimeoutError, match="diagnostics"):
        driver.acquire(2, timeout=1)
    assert driver.blocks == []
    assert driver.stops == 2


@pytest.mark.parametrize("samples", [0, 1, 3, -2, adc_dma.MAX_SAMPLES + 2, 2.5])
def test_invalid_sample_count_does_not_touch_hardware(samples):
    driver = FakeDma()
    with pytest.raises(ValueError):
        driver.acquire(samples)
    assert driver.stops == 0


def test_wrong_clock_rejected_before_arming():
    driver = FakeDma()
    driver.get_adc_sampling_freq = lambda: np.array([200e6, 250e6])
    with pytest.raises(RuntimeError, match="Both ADC clocks"):
        driver.acquire(2)
    assert driver.blocks == []
    assert driver.stops == 2


def test_repeated_whole_dma_packet_rejected():
    driver = FakeDma()
    original = driver.get_adc_block
    driver.get_adc_block = lambda pair, index: original(pair, 0 if index == 1 else index)
    with pytest.raises(RuntimeError, match="channel 1, sample 65536"):
        driver.acquire(2 * adc_dma.PACKET_SAMPLES, test_pattern=True)
    assert driver.stops == 2
