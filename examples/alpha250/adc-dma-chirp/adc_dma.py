"""Thin client for one contiguous ADC0 acquisition."""
import time

import numpy as np
from koheron import command

N_SAMPLES = 64 * 1024 * 1024
N_DESCRIPTORS = 512


class AdcDma:
    def __init__(self, client):
        self.client = client

    @command()
    def configure(self, samples, coefficient, seeds):
        return self.client.recv_bool()

    @command()
    def start(self):
        return self.client.recv_bool()

    @command()
    def get_status(self):
        return self.client.recv_uint32()

    @command()
    def get_adc_block(self, index):
        return self.client.recv_vector(dtype="uint32", check_type=False)

    @command()
    def stop(self):
        pass

    def acquire(self, chirp, timeout=5.0):
        try:
            if not self.configure(chirp.samples, chirp.coefficient,
                                  np.asarray(chirp.seeds, dtype=np.uint64)):
                raise RuntimeError("Invalid chirp settings or DMA reset timeout")
            if not self.start():
                raise RuntimeError("Acquisition was not armed")
            deadline = time.monotonic() + timeout
            while True:
                status = self.get_status()
                if status == 1:
                    break
                if status:
                    errors = {2: "ADC FIFO overflow", 3: "ADC near full scale (possible clipping)", 4: "DMA error"}
                    raise RuntimeError(errors.get(status, f"Unknown status {status}"))
                if time.monotonic() >= deadline:
                    raise TimeoutError("ADC acquisition did not complete")
                time.sleep(0.01)
            adc = np.empty(N_SAMPLES, dtype=np.int16)
            block_samples = N_SAMPLES // N_DESCRIPTORS
            for index in range(N_DESCRIPTORS):
                block = self.get_adc_block(index).view("<i2")
                if len(block) != block_samples:
                    raise RuntimeError("Incomplete ADC data block")
                adc[index * block_samples:(index + 1) * block_samples] = block
            return adc
        finally:
            self.stop()
