# ADC/DAC BRAM loopback

Connect DAC0 to ADC0 for `validate_loopback.py`.

## Sampling rate

Set `parameters.adc_clk` in `memory.yml` before building the instrument. The
ALPHA250 clock driver starts at this frequency; FPGA clock configuration, server,
and timing constraints must be built together. Supported build rates are
100, 200, 240, and 250 MHz. The default example remains 250 MHz.

For a 100 MHz build, set `adc_clk: 100000000`, then build and load the instrument:

```sh
make CFG=examples/alpha250/adc-dac-bram/config.mk ENFORCE_TIMING=1
```

`ClockGenerator.set_sampling_frequency()` accepts only the rate matching the
build. Selecting another rate logs an error and leaves the current rate unchanged.
A new sampling rate requires a matching FPGA build: this API does not reconfigure
the MMCM dividers or regenerate timing constraints. Selecting the current rate
is a no-op, including immediately after startup.

The 100 MHz build uses 300 MMCM fine-phase increments (5.357 ns at its 1 GHz
MMCM VCO). Its DAC transfer constraint reserves 5.358 ns of setup time while
retaining the unshifted hold requirement. The previous 120 increments put the
100 MHz BRAM loopback outside its measured clean capture window.

## Validation

After loading or restarting the instrument, run:

```sh
HOST=192.168.1.105 python3 validate_loopback.py
```

The script uses the startup phase without adjustment. It tests coherent tones
from 1 through 50 MHz, excluding frequencies at or above Nyquist, with 20 captures
per tone. At 100 MS/s this checks 13,107,200 samples. Repeat after instrument
restarts to check initialization as well as steady-state captures.

Results and representative raw captures are saved in `loopback-validation/`.
The pass criteria are a fitted carrier amplitude of at least 1000 ADC codes
and no sine-fit residual over 200 codes. This is an analog
loopback check on the connected board, not a guarantee over all boards or
temperatures.

The current BRAM read method triggers a capture and immediately reads memory.
The script therefore primes the buffer with each periodic tone, waits for an RPC
reply confirming the trigger was processed, then waits 10 ms before readout.
This avoids treating stale data from the previous tone as a clock-phase failure;
it does not test acquisition-completion handling.
