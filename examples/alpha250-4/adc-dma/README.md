# Four-channel ADC DMA

ALPHA250-4 one-shot acquisition at **200 MS/s on all four RF inputs**, using
**16-bit storage per ADC code**. The default record is **16,000,000 samples
per channel**, or **80 ms** and **128,000,000 bytes** total. Even lengths from
2 through 16,777,216 samples are supported; the maximum occupies 128 MiB.

Two receive-only AXI DMAs write through HP0 and HP2, each handling two
channels at 800 MB/s. Each stream packs two successive sample times into a
64-bit beat without changing the 16-bit codes. A common trigger and sample
counter keep the digital capture aligned. Two 64 KiB FIFOs absorb short DDR
stalls (about 81.9 microseconds at 800 MB/s). A FIFO overflow rejects the
record. The remaining DMA slots are padded with zeros so both finite chains
finish before the client reports the error; padded data is never downloaded
as a successful capture. Write-data FIFOs buffer complete AXI bursts before
forwarding them to the HP ports. Hardware results below cover the default
and maximum record lengths.

## Build

From the SDK root, using the installed Vivado version:

```sh
make validate CFG=examples/alpha250-4/adc-dma/config.mk
make -j4 CFG=examples/alpha250-4/adc-dma/config.mk N_CPUS=8
make timing CFG=examples/alpha250-4/adc-dma/config.mk N_CPUS=8
```

The instrument archive is `tmp/examples/alpha250-4/adc-dma/adc-dma.zip`.
`ENFORCE_TIMING := 1` requires constrained routed timing checks to pass.
The inherited board I/O constraints still require physical ADC validation;
a shared digital trigger does not independently calibrate analog skew
between the two ADC chips.

## Boot image and DMA memory

Use a **V1 OS image**, with the exclusive fixed DMA reservation from
`boards/alpha250/config/board.dtso` (also inherited by ALPHA250-4). Before
deploying, verify on the board:

```sh
sudo cat /proc/iomem
test -e /proc/device-tree/reserved-memory/dma-buffer@18000000/no-map
```

System RAM must exclude **`0x18000000..0x1fffffff`**. The two capture windows
are `0x18000000..0x1bffffff` (IN0/IN1) and `0x1c000000..0x1fffffff`
(IN2/IN3). The server checks both against `/proc/iomem` before mapping them
through `mem_wc`, including its `/dev/mem` fallback. An instrument ZIP cannot
reserve memory on an older image: update the boot image and reboot if needed.
See `os/tests/dma-reservation/README.md`.

Each DMA has up to 256 descriptors of 256 KiB; a shorter final descriptor
supports exactly 16 million samples. Descriptor chains live in separate
16 KiB OCM windows (`0xffff0000` and `0xffff4000`), outside DDR. The driver
maps the last OCM bank high without changing the other banks' mappings.
Transfers are finite rather than cyclic. Every descriptor's completion,
error bits and byte count are checked before any record is downloaded.
The instrument overlay disables Linux DMAengine binding for these two
controllers; their registers and descriptor chains are owned by the server.

## Deploy and try

```sh
make run CFG=examples/alpha250-4/adc-dma/config.mk HOST=BOARD_IP N_CPUS=8
```

Ctrl+C stops the log stream and leaves the instrument running.

First test the DMA and memory path with FPGA counter patterns replacing the
ADC data. Each pair encodes the full sample index across its two 16-bit codes,
so the check detects both counter rollovers and repeated/reordered DMA blocks.
Repeat acquisitions exercise resets and stale descriptor handling:

```sh
.venv/bin/python examples/alpha250-4/adc-dma/test.py --host BOARD_IP --test-pattern --repeat 10
```

Then capture all four real ADC inputs:

```sh
.venv/bin/python examples/alpha250-4/adc-dma/test.py --host BOARD_IP --output capture.npy
```

The client explicitly selects the board's 200 MHz clock configuration. The
reference clock remains as initialized by the board's Common driver (TCXO).
Download begins after both DMAs finish, avoiding Ethernet reads during capture.
No second full-size capture array is allocated on the board. Host output is
an `(N, 4)` NumPy array of little-endian `int16`, columns **IN0, IN1, IN2, IN3**.
ADC codes retain the existing board interface's left-aligned 14-bit format
(two low bits zero). For right-aligned signed 14-bit codes use `data >> 2`.

```python
import numpy as np
data = np.load('capture.npy', mmap_mode='r')
channel0 = data[:, 0]
```

Output files are optional; existing files are refused and failed captures
remove their incomplete files. `--samples 16777216` uses the complete 128 MiB
buffer and captures 83.88608 ms. The printed acquisition/download time includes
clock settling, software polling and network transfer; it is not the ADC
record duration. FPGA counter mode validates the capture/DMA path, not the
physical ADC interface or analog accuracy.

Counter verification checks every sample index in both streams, including
counter rollovers and DMA packet boundaries. It detects missing, repeated or
reordered indices within the tested records. The counters replace data after
the ADC receiver, so they cannot detect conversions lost or repeated upstream
of that point. Real ADC records are checked for capture overflow, DMA errors
and exact descriptor byte counts; their analog continuity needs separate
validation. Acquisition stops between records, so separate triggers have gaps.

For separate arming and triggering, call `configure(samples, test_pattern)`,
`arm()` and `trigger()` in that order; each returns a success flag. `start()`
combines the last two steps. `get_status()` and `get_diagnostics()` remain
available for checking completion and diagnosing rejected records.

## Offline checks

```sh
.venv/bin/python -m pytest examples/alpha250-4/adc-dma/tests -q
g++ -std=c++23 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -Iexamples/alpha250-4/adc-dma/tests/stubs \
    examples/alpha250-4/adc-dma/tests/test_driver.cpp -o /tmp/quad-dma-driver-test
/tmp/quad-dma-driver-test
source /tools/Xilinx/2025.1/Vivado/settings64.sh
vivado -mode batch -nolog -nojournal -source examples/alpha250-4/adc-dma/tests/run_sim.tcl
```

RTL simulation covers a shared sample-zero trigger, four-channel analog-code
ordering, independent short stalls, full and partial packets, minimum records,
repeated acquisition, counter rollover, both overflow paths, stable AXIS data
under backpressure and invalid lengths. Python tests cover channel order,
partial final downloads, memmap output, data corruption, incomplete blocks,
hardware errors, timeouts and cleanup. Native C++ tests exercise the actual
driver with simulated registers, checking both SG chains at the requested
and maximum sizes, finite tails, reset timeout and descriptor errors.

Build validation and physical hardware results are reported separately.

Validation of the FPGA implementation with Vivado 2025.1 on XC7Z020-2 passed
bitstream generation and the strict routed timing check: setup slack +0.514 ns,
hold slack +0.031 ns, and all 10 bus-skew constraints met. Placed utilization
was 6,093 LUTs, 11,106 registers, 48 block RAM tiles and no DSPs. The inherited
board constraints leave 28 inputs and 9 outputs without delay constraints; these results do not
establish physical ADC timing or capture reliability. The ARM server compiled,
the RTL simulation, 17 Python tests and sanitized native C++ driver tests passed,
and full-size synthetic client downloads verified every pattern code at both
16,000,000 and 16,777,216 samples per channel.

## Hardware validation

On 2026-10-05 the instrument was uploaded to ALPHA250-4 at 192.168.1.12,
running a V1 OS image. Its reserved-memory checks passed, and the design with
OCM descriptors, write-data FIFOs and overflow draining passed the following
tests at 200 MS/s on all four channels:

| Record length per channel | Counter captures | Result |
| --- | --- | --- |
| 2 | 2 | Every code and descriptor verified |
| 8,192 | 2 | Every code and descriptor verified |
| 65,542 | 2 | Every code and descriptor verified, including the partial final packet |
| 16,000,000 | 3 | Every code and descriptor verified; 80 ms records |
| 16,777,216 | 2 | Every code and descriptor verified; full 128 MiB buffer |

One real ADC capture also completed at 16,000,000 samples per channel. It was
saved locally as `tmp/examples/alpha250-4/adc-dma/hardware-capture-200msps.npy`:
shape `(16000000, 4)`, little-endian `int16`, 128,000,000 data bytes. Every
code retained the ADC interface's two zero low bits, and all four channels
showed the connected approximately 10 MHz signal. These checks establish
successful capture and transfer for the trials above; they do not calibrate
analog accuracy or inter-channel skew. Full-record acquisition and download
took approximately 2.5–3.1 seconds, including clock settling and host checking.

The CLI bounds individual RPCs with `--timeout`, as well as its capture-status
polling loop. Overflow draining is covered by RTL simulation; the successful
hardware trials did not deliberately induce an overflow.

These hardware results precede the driver/client cleanup. That cleanup leaves
the FPGA implementation unchanged; its validation uses the offline tests and
ARM server build above.
