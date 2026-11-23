"""Python interface to the libmetal SPI internal-loopback instrument."""

import numpy as np
from koheron import command


class LibmetalSpi:
    def __init__(self, client):
        self.client = client

    @command()
    def is_ready(self):
        return self.client.recv_bool()

    @command()
    def get_error(self):
        return self.client.recv_string()

    @command()
    def get_interrupt_count(self):
        return self.client.recv_uint32()

    @command()
    def get_last_duration_us(self):
        return self.client.recv_uint32()

    def transfer(self, data, timeout_ms=2000):
        """Return looped-back bytes, or raise on a device error or timeout."""
        data = np.asarray(data)
        if data.ndim != 1 or not 1 <= data.size <= 65536:
            raise ValueError("Expected a one-dimensional array of 1..65536 bytes")
        if not np.issubdtype(data.dtype, np.integer) or np.any(data < 0) or np.any(data > 255):
            raise ValueError("SPI data must contain integers in 0..255")
        if not isinstance(timeout_ms, int) or not 1 <= timeout_ms <= 10000:
            raise ValueError("timeout_ms must be an integer in 1..10000")
        received = self._transfer(np.ascontiguousarray(data, dtype=np.uint8), timeout_ms)
        if received.size != data.size:
            raise RuntimeError(self.get_error() or "Incomplete SPI transfer")
        return received

    @command(funcname="transfer")
    def _transfer(self, data, timeout_ms):
        return self.client.recv_vector(dtype="uint8")
