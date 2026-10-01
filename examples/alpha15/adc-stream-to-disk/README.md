# ALPHA15 ADC0 recording

This example records ADC0 at 15 MS/s on a PC. Each sample is one 32-bit word:
the lower 18 bits are the ADC value and the upper 14 bits are a rolling sample
number. The FPGA uses AXI DMA and a 16 MiB DDR ring; the Koheron server sends
four 512 KiB DMA blocks per request over plain TCP. The PC writes the data to
disk. DAC0 can generate a test tone for an ADC0 loopback cable.

## Build and install

From the SDK root:

```sh
BOARD_IP=192.168.1.105  # use your board's address
make CFG=examples/alpha15/adc-stream-to-disk/config.mk
make run CFG=examples/alpha15/adc-stream-to-disk/config.mk HOST="$BOARD_IP"
```

Press Ctrl+C to leave the log viewer. The instrument continues to run on the
board.

## Record a file

Run on the PC, from the SDK root:

```sh
PYTHONPATH=python python3 examples/alpha15/adc-stream-to-disk/record.py \
  "$BOARD_IP" capture.raw --seconds 60
python3 examples/alpha15/adc-stream-to-disk/verify_sequence.py capture.raw
```

With DAC0 connected to ADC0, add `--test-tone` to drive a 7.324 kHz triangle
while recording. The recorder turns the tone off when it stops.

The recorder writes `capture.raw.partial` during acquisition and renames it to
`capture.raw` only after the hardware reports no error. A failed run leaves the
partial file for inspection. Ctrl+C stops the recorder cleanly after the
current 2 MiB batch.

## Keep the latest 60 seconds on disk

```sh
PYTHONPATH=python python3 examples/alpha15/adc-stream-to-disk/record.py \
  "$BOARD_IP" latest.raw --ring-seconds 60
# Press Ctrl+C when ready to stop.
python3 examples/alpha15/adc-stream-to-disk/verify_sequence.py latest.raw
```

The recorder keeps writing until Ctrl+C. It preallocates one disk file and
overwrites its oldest 2 MiB batch when full. For a 60-second ring, the file is
3,600,809,984 bytes and holds about 60.01 seconds, rounded to a whole batch.
`latest.raw.json` records the position of the oldest batch. The verifier uses
this file to check samples in time order, including across the wrap. You can
also add `--seconds N` to stop the ring automatically after N seconds.

## Read the samples

For a normal file, use little-endian unsigned 32-bit words:

```python
import numpy as np
words = np.fromfile("capture.raw", dtype="<u4")
raw18 = words & 0x3ffff
sequence = words >> 18
```

For a ring file, physical order may start in the middle of the capture. Read
`latest.raw.json` and start at batch
`completed_batches % capacity_batches` once the ring has wrapped. The verifier
does this automatically. Use the existing ALPHA15 conversion routine to turn
the lower 18 bits into signed values or volts.

## Throughput and checks

At 15 MS/s the PC must receive and save 60 MB/s. The FPGA's 16 MiB ring holds
about 0.28 seconds. If the DMA ring fills, an ADC conversion meets
backpressure, or a descriptor is short, the recorder reports an error.

The 14-bit number increments on every ADC conversion, even if that conversion
cannot enter the DMA stream. `verify_sequence.py` checks every adjacent pair,
including batch boundaries and disk-ring wrap. A loss of a multiple of 16,384
samples could evade the tag check, so the hardware error checks must also
remain clear.

On the tested ALPHA15 and PC, a 60-second loopback recording saved
3,586,129,920 bytes at 59.8 MB/s. All 896,532,480 saved samples passed the
sequence check, with no hardware error. A 75-second ring run received
4,494,196,736 bytes, retained the latest 3,600,809,984 bytes, and passed the
sequence check across the wrap for all 900,202,496 retained samples. Verify
longer recordings on the intended PC and disk.
