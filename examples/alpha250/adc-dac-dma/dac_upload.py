"""Acknowledged uploads to the ALPHA250 DMA waveform RAM."""
import numpy as np
from koheron import command

N_PTS = 64 * 1024
N_DESC_MAX = 256
# Leave room for the RPC header, offset and length within WebSocket frames too.
DAC_CHUNK_WORDS = 32 * 1024


class DacUpload:
    def set_dac_data(self, data):
        """Write packed uint32 samples, waiting for every chunk before returning.

        Upload while DMA is stopped. A failed upload can leave earlier chunks
        written; retry the complete waveform before starting an acquisition.
        """
        data = np.asarray(data)
        if data.ndim != 1:
            raise ValueError('DAC data must be a one-dimensional array')
        if data.dtype != np.dtype('uint32'):
            raise TypeError('DAC data must contain packed uint32 words')
        if data.size > N_PTS * N_DESC_MAX:
            raise ValueError('DAC data exceeds the 64 MiB waveform RAM')
        for offset in range(0, data.size, DAC_CHUNK_WORDS):
            chunk = np.ascontiguousarray(data[offset:offset + DAC_CHUNK_WORDS])
            if not self.set_dac_data_chunk(offset, chunk):
                raise RuntimeError(f'DAC upload rejected at word offset {offset}')

    @command(classname='AdcDacDma')
    def set_dac_data_chunk(self, offset, data):
        return self.client.recv_bool()
