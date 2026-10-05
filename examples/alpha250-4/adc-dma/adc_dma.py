"""Four simultaneous ADC channels, 200 MS/s, 16-bit codes, finite SG capture."""
import time

import numpy as np
from koheron import command

DEFAULT_SAMPLES = 16_000_000
MAX_SAMPLES = 16 * 1024 * 1024
PACKET_SAMPLES = 64 * 1024
SAMPLE_RATE = 200_000_000
STATUS_ERRORS = {
    2: "IN0/IN1 FIFO overflow",
    3: "IN2/IN3 FIFO overflow",
    4: "DMA/descriptor error",
    5: "Invalid capture settings",
    6: "ADC clock unlocked",
}


def validate_samples(samples):
    if not isinstance(samples, (int, np.integer)) or samples < 2 or samples > MAX_SAMPLES or samples % 2:
        raise ValueError(f"samples must be even and between 2 and {MAX_SAMPLES}")


def check_pattern(block, first_sample, pair):
    """Check every frame, including rollover and differences between channels."""
    indices = np.arange(first_sample, first_sample + len(block), dtype=np.uint32)
    for channel in range(2):
        codes = (indices if channel == 0 else indices >> 16).astype(np.uint16)
        expected = codes ^ np.uint16((2 * pair + channel) * 0x4000)
        mismatch = np.flatnonzero(block[:, channel].view(np.uint16) != expected)
        if len(mismatch):
            i = int(mismatch[0])
            raise RuntimeError(f"Pattern mismatch: channel {2 * pair + channel}, sample {first_sample + i}")


class AdcDma:
    def __init__(self, client):
        self.client = client

    @command()
    def get_info(self):
        return self.client.recv_array(3, dtype="uint32")

    @command()
    def configure(self, samples, test_pattern):
        return self.client.recv_bool()

    @command()
    def start(self):
        return self.client.recv_bool()

    @command()
    def arm(self):
        return self.client.recv_bool()

    @command()
    def trigger(self):
        return self.client.recv_bool()

    @command()
    def get_status(self):
        return self.client.recv_uint32()

    @command()
    def get_diagnostics(self):
        return self.client.recv_array(6, dtype="uint32")

    @command()
    def get_adc_block(self, pair, index):
        return self.client.recv_vector(dtype="uint32", check_type=False)

    @command()
    def stop(self):
        pass

    @command(classname="ClockGenerator")
    def set_sampling_frequency(self, selection):
        pass

    @command(classname="ClockGenerator")
    def get_adc_sampling_freq(self):
        return self.client.recv_array(2, dtype="float64")

    def acquire(self, samples=DEFAULT_SAMPLES, test_pattern=False, timeout=5.0, output=None):
        """Return (samples, 4) int16 codes; output optionally supplies that array.

        output can be a NumPy memmap to save directly to a host file. Data is
        read only after both finite DMA chains have completed and been checked.
        """
        validate_samples(samples)
        if timeout <= 0:
            raise ValueError("timeout must be positive")
        if output is not None and (output.shape != (samples, 4) or output.dtype != np.dtype("<i2")):
            raise ValueError("output must have shape (samples, 4) and little-endian int16 dtype")
        try:
            self.stop()
            self.set_sampling_frequency(0)  # ALPHA250-4 clock table: 200 MHz.
            time.sleep(0.05)
            if not np.all(self.get_adc_sampling_freq() == SAMPLE_RATE):
                raise RuntimeError("Both ADC clocks must be configured to 200 MHz")
            if not self.configure(samples, bool(test_pattern)):
                raise RuntimeError("Cannot configure acquisition (DMA reset or ADC clock lock)")
            if not self.start():
                raise RuntimeError("Acquisition was not armed or ADC clock is unlocked")
            self._wait_for_completion(timeout)
            return self._download(samples, test_pattern, output)
        finally:
            self.stop()

    def _wait_for_completion(self, timeout):
        deadline = time.monotonic() + timeout
        while True:
            status = self.get_status()
            if status == 1:
                return
            if status:
                error = STATUS_ERRORS.get(status, f"Unknown status {status}")
                raise RuntimeError(f"{error}; diagnostics={self.get_diagnostics()}")
            if time.monotonic() >= deadline:
                raise TimeoutError(f"Acquisition timed out; diagnostics={self.get_diagnostics()}")
            time.sleep(0.005)

    def _download(self, samples, test_pattern, output):
        adc = np.empty((samples, 4), dtype="<i2") if output is None else output
        for index, first in enumerate(range(0, samples, PACKET_SAMPLES)):
            frames = min(PACKET_SAMPLES, samples - first)
            for pair in range(2):
                words = self.get_adc_block(pair, index)
                if len(words) != frames:
                    raise RuntimeError(f"Incomplete ADC block: pair {pair}, block {index}")
                block = words.astype("<u4", copy=False).view("<i2").reshape(frames, 2)
                if test_pattern:
                    check_pattern(block, first, pair)
                adc[first:first + frames, 2 * pair:2 * pair + 2] = block
        return adc
