# ALPHA250 ADC/DAC DMA

Build with `make CFG=examples/alpha250/adc-dac-dma/config.mk`.
The routed timing check is mandatory for this instrument.

DMA uses three independent AXI connections, each with registered input and
output channels:

| DMA interface | Destination | Data width |
| --- | --- | --- |
| Scatter-gather descriptors | PS GP0, on-chip memory | 32 bits |
| DAC reads (MM2S) | PS HP0, reserved DDR | 64 bits |
| ADC writes (S2MM) | PS HP2, reserved DDR | 64 bits |

The memory addresses, DMA burst lengths, ADC/DAC rates and client interface
are unchanged. Dedicated connections remove arbitration between unrelated
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
