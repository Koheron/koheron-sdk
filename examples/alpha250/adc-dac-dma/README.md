# ALPHA250 ADC/DAC DMA

Build with `make CFG=examples/alpha250/adc-dac-dma/config.mk`.
The routed timing check is mandatory for this instrument.

`test.py` and `bode_analyzer.py` upload packed `uint32` waveforms using
`set_dac_data_chunk(offset, data)`. Offsets count words, and each 128 KiB chunk
is acknowledged before the next is sent. The complete waveform may occupy up
to the 64 MiB DAC region in `memory.yml` (256 descriptors). The server checks
the entire requested range before writing and returns `false` on an invalid
range. These clients require instrument version 0.2.3 or later.

Upload while DMA is stopped, and start acquisition only after `set_dac_data`
returns successfully. If a chunk fails or the connection closes, earlier
chunks may already have been written; reconnect as needed and upload the
complete waveform again. Uploads are not atomic across multiple clients.

The legacy void `set_dac_data` RPC remains available for uploads of at most
65,536 words (256 KiB) over TCP. Larger requests exceed the server's argument
limit and close the connection. The shared fixed command buffer is unchanged.

Local upload regressions (the C++ tests need the SDK compiler dependencies):

```sh
PYTHONPATH=python .venv/bin/python3 -m unittest discover -s python/tests -p test_dma_upload.py
CXX=g++-15 python3 -m unittest discover -s server/tests -p test_dma_upload.py
CXX=g++-15 python3 -m unittest discover -s server/tests -p test_driver_locking.py
```

DMA uses three independent AXI connections, each with registered input and
output channels:

| DMA interface | Destination | Data width |
| --- | --- | --- |
| Scatter-gather descriptors | PS GP0, on-chip memory | 32 bits |
| DAC reads (MM2S) | PS HP0, reserved DDR | 64 bits |
| ADC writes (S2MM) | PS HP2, reserved DDR | 64 bits |

The dedicated connections preserve the memory addresses, DMA burst lengths
and ADC/DAC rates. They remove arbitration between unrelated
address spaces and the descriptor path's unnecessary 32→64→32 conversion.
Registered AXI channels add latency while continuing to accept one beat per
cycle when ready.

DAC samples cross from the ADC clock into the DAC clock through a 16-word
Xilinx XPM asynchronous FIFO. The final output register and SelectIO use the
same DAC clock, allowing a full cycle for output setup. Both DACs share the
FIFO and retain separate output registers, keeping their sample sequences
aligned. FIFO pointer
synchronizers and their vendor timing constraints handle the clock crossing;
the existing ADC phase allowance is retained, with no new manual exceptions.
The FIFO resets with the acquisition stream and emits zero while empty.

Local simulation measures seven additional DAC-clock cycles versus the
original two-stage delay: 28 ns at 250 MS/s, or 35 ns at 200 MS/s. It preserves
sample order and one sample per clock after startup. Account for this extra
delay when aligning the DAC waveform with ADC acquisition.
No additional MMCM output or global clock buffer is required.

Run the sample-order, reset and latency regression locally:

```sh
bash examples/alpha250/adc-dac-dma/tests/run-fpga.sh
```

The simulation checks continuous sample ordering and measures added latency at
200 and 250 MS/s, with ADC phase offsets of 0–1 ns at 250 MS/s and
0–1.25 ns at 200 MS/s. It is separate from the quick CI smoke tests.
