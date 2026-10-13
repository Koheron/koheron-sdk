#!/usr/bin/env python3
"""Board-only loopback test: no SPI wiring, sensor, or test equipment required."""

import os

import numpy as np
from koheron import connect

from client import LibmetalSpi


def main():
    host = os.environ.get("HOST", "192.168.1.100")
    client = connect(host, "libmetal-spi")
    # Bound network waits as well as the server's SPI completion wait.
    client.sock.settimeout(15)
    try:
        spi = LibmetalSpi(client)
        if not spi.is_ready():
            raise RuntimeError(spi.get_error())
        rng = np.random.default_rng(870)
        total = 0
        # Include short transfers, FIFO boundaries (depth=16), and repeated refills.
        for size in (1, 15, 16, 17, 31, 32, 33, 257, 4096, 65536):
            patterns = (
                np.zeros(size, dtype=np.uint8),
                np.full(size, 255, dtype=np.uint8),
                (np.arange(size, dtype=np.uint32) & 255).astype(np.uint8),
                rng.integers(0, 256, size=size, dtype=np.uint8),
            )
            for data in patterns:
                before = spi.get_interrupt_count()
                received = spi.transfer(data, timeout_ms=10000)
                np.testing.assert_array_equal(received, data)
                interrupts = (spi.get_interrupt_count() - before) & 0xFFFFFFFF
                if interrupts == 0:
                    raise RuntimeError("Transfer completed without a libmetal IRQ callback")
                total += data.size
            print(f"PASS {size:5d} bytes: four patterns; last transfer "
                  f"{spi.get_last_duration_us()} us, {interrupts} IRQ callbacks")
        print(f"PASS: 40 transfers, {total} bytes verified through internal SPI loopback")
    finally:
        client.sock.close()


if __name__ == "__main__":
    main()
