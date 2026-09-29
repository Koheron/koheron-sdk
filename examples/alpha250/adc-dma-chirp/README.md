# ADC DMA chirp

One-shot ALPHA250 loopback frequency-response measurement. DAC0 generates a
sample-by-sample logarithmic sine chirp while ADC0 records **64 Mi samples**
into the entire **128 MiB** DMA buffer. At 250 MS/s, the capture lasts 268.4 ms.
DAC1 stays at zero. No DAC waveform is stored in DDR.

The example uses a 250 ms sweep from 500 Hz to 110 MHz, and plots magnitude
and wrapped phase from **1 kHz to 100 MHz** on a logarithmic frequency axis.
The extra bandwidth keeps the displayed response away from the sweep edges.
Short, approximately 1 ms linear amplitude tapers suppress start/stop
discontinuities. The sine has full digital amplitude between the tapers.

## Files

| File | Purpose |
| --- | --- |
| `test.py` | Short example with hard-coded settings and two response plots |
| `adc_dma.py` / `adc_dma.hpp` | Acquisition, completion/error checks, data transfer |
| `chirp.py` | Sweep settings, deterministic reference, whole-record FFT ratio |
| `block_design.tcl` / `memory.yml` | FPGA connections and memory map |
| `chirp_generator_v1_0/` | Pipelined logarithmic phase generator |
| `adc_capture_v1_0/` | DAC tapers, aligned capture, packing and error flags |
| `tests/` | Offline Python tests and Vivado simulations |

## Build without accessing a board

From the SDK root:

```sh
make fpga CFG=examples/alpha250/adc-dma-chirp/config.mk VIVADO_VERSION=2026.1 N_CPUS=8
make timing CFG=examples/alpha250/adc-dma-chirp/config.mk VIVADO_VERSION=2026.1
```

Use the installed Vivado version. `ENFORCE_TIMING := 1` prevents a bitstream
build from succeeding with constrained routed timing violations. The example
inherits the ALPHA250 board I/O constraints; the SDK timing checker also reports
any unconstrained board interfaces. Passing its timing check does not replace
physical board validation.

The server can be cross-compiled without deployment:

```sh
make tmp/examples/alpha250/adc-dma-chirp/serverd CFG=examples/alpha250/adc-dma-chirp/config.mk N_CPUS=8 -j8
```

## Running on a board

After installing the instrument separately, connect **OUT0 to IN0** with a
50-ohm coaxial cable.
The expected sampling rate is 250 MS/s and the default DAC analog gain is 1 Vpp
into 50 ohms. Memory access follows the existing `adc-dac-dma` example:
fixed physical DDR addresses and DMA descriptors in high OCM, with no
`/dev/cma` allocation or boot-configuration changes. The chirp uses the entire
DDR range `0x18000000..0x1fffffff` for ADC capture, instead of splitting it
into 64 MiB DAC and ADC buffers. This retains the existing example's assumption
that the DDR range is exclusively available; it does not claim ownership from
Linux when that region is configured as a reusable CMA pool.

Edit the host and the `Chirp(start_hz=..., stop_hz=..., duration=...)` line in
`test.py`, then run:

```sh
.venv/bin/python -m pip install --no-deps -e python
.venv/bin/python examples/alpha250/adc-dma-chirp/test.py
```

Dependencies are the SDK's Python client, NumPy, SciPy and Matplotlib. The
script explicitly selects 250 MS/s, captures once, downloads the record in
256 KiB blocks, reconstructs the reference and plots the response. Computing
the reference and FFTs happens on the host. Allow approximately 2 GiB of free
host memory; the full reference took about 9 seconds on the development host.
The board does not allocate a second 128 MiB copy of the capture.

The client waits for DMA completion and checks every descriptor's status and
transferred length before reading. It rejects a timeout, FIFO overflow or ADC
near-rail samples (possible clipping). It does not silently lower the requested
full DAC amplitude. The clipping flag does not detect all analog distortion.

## Signal generation and phase reference

For `N` excitation samples, Python calculates `r = (f_stop/f_start)^(1/(N-1))`
with high precision. The generator uses 16 interleaved 64-bit states: a 48-bit
DDS frequency increment plus 16 fractional guard bits. With
`C = round((r^16 - 1) * 2^64)`, each state advances as
`X_next = X + round(X*C/2^64)`. One shared pipelined multiplier engine emits
the next increment on **every clock**. Frequencies are not held for 16 samples.

The phase accumulator is 48 bits. Its upper 16 bits address a 16-bit DDS sine
LUT with phase dithering disabled. An excitation-valid tag travels through
the DDS using TUSER; the LUT keeps clocking after the sweep to drain all samples.
The capture starts at the first tapered sample on the digital DAC port,
independently of the DDS/taper pipeline latency, and continues through the
remaining response tail. About 18.4 ms of silence follows the default sweep.

Python reconstructs the exact integer frequency and phase recurrence, phase
truncation and taper arithmetic. The sine model is
`round(32766 * sin(2*pi*address/65536))`; `run_dds_sim.tcl` checks exact equality
at all 65,536 LUT addresses for the configured DDS IP. The phase reference is the
FPGA's digital DAC port, so the reported phase includes the downstream FPGA
I/O registers, DAC, cable, ADC and ADC interface delay. No arbitrary time shift
or delay removal is applied.

The transfer estimate is `FFT(ADC)/FFT(reference)` at selected logarithmically
spaced FFT bins. It uses the complete capture, including the response tail,
without Welch segmentation or a second whole-record window. Low-excitation
bins are rejected. Phase is displayed wrapped because arbitrary delays can
make unwrapping a sparse logarithmic grid ambiguous. Magnitude is a ratio of
normalized ADC and DAC codes, not an independently calibrated analog voltage
measurement. Quantization, noise, distortion and a response extending beyond
the captured tail limit measurement accuracy.

## Offline checks

```sh
.venv/bin/python -m pytest examples/alpha250/adc-dma-chirp/tests -q
source /tools/Xilinx/2026.1/Vivado/settings64.sh
vivado -mode batch -nolog -nojournal -source examples/alpha250/adc-dma-chirp/tests/run_sim.tcl
vivado -mode batch -nolog -nojournal -source examples/alpha250/adc-dma-chirp/tests/run_dds_sim.tcl
```

The RTL tests check integer recurrence, repeat acquisitions, exact sample and
packet counts, sample ordering under stalls, overflow, clipping, tapers and
return to zero. The DDS simulation checks sine values and propagation of all
valid tags through the actual AMD IP. Python tests include a known filter,
gain and delay. None of these tests connects to a board.

Validated offline with Vivado 2026.1 on XC7Z020-2: bitstream generation passed
with setup slack +0.145 ns, hold slack +0.039 ns and all eight bus-skew
constraints met. Placed utilization was 5,582 LUTs, 8,942 registers, 17.5 block
RAM tiles and 10 DSPs. The inherited board constraints leave 14 input and 25
output ports without delay constraints. The ARM server cross-compiled; all
three Vivado testbenches and eight Python tests passed. An additional full-size
synthetic filter/delay check verified the whole-record processing.

An initial ALPHA250 DAC0-to-ADC0 hardware run acquired and downloaded all
67,108,864 samples in approximately 2.4 seconds, with every DMA descriptor
complete and no FIFO-overflow or near-rail flags. ADC extrema were -31,344
and 31,904 codes. The estimated magnitude was approximately -0.34 dB at
100 kHz and -4.83 dB at 100 MHz. This verifies the acquisition and processing
path; analog accuracy has not been checked against an independent instrument.

A subsequent capture with an external low-pass filter, divided by the direct
loopback response, showed a -3 dB cutoff near 25.7 MHz, approximately 0.05 dB
low-frequency insertion loss, and about 59 dB attenuation at 50 MHz. An
anomalous response point at 100 MHz remains unexplained; the upper-band result
and deep-stopband phase should not be treated as validated filter measurements.
