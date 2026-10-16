# BIST Full

`bist-full` is an extended validation instrument for the original Red Pitaya
STEMlab 125-14 and the V1 OS. Each full client validation has a **60-second
end-to-end deadline**, including reboot, network/RPC readiness, onboard tests,
SDK protocol checks and service/overlay recovery. It is a separate instrument
and a new FPGA design; the compact `bist` remains the fixed performance reference.

The default wiring is **DAC0 → ADC0**, with channel 1 unconnected. Use the LV
input range. Unconnected converter channels are explicitly skipped. The other
DAC stays quiet during each connected-channel sweep, so crossed wiring cannot
pass by receiving an identical signal from the wrong output.

## Build and validate

```sh
make doctor TARGET=all CFG=examples/red-pitaya/bist-full/stemlab-125-14.mk
make info CFG=examples/red-pitaya/bist-full/stemlab-125-14.mk FORMAT=json
make -j8 N_CPUS=8 CFG=examples/red-pitaya/bist-full/stemlab-125-14.mk
make deploy CFG=examples/red-pitaya/bist-full/stemlab-125-14.mk HOST=192.168.1.85
BIST_SSH_KNOWN_HOSTS=path/to/known_hosts \
  make test CFG=examples/red-pitaya/bist-full/stemlab-125-14.mk HOST=192.168.1.85
```

Install `paramiko` in the SDK Python environment for reboot/recovery SSH.
`BIST_SSH_PASSWORD` overrides the image's root password; SSH keys also work.
The full test makes `bist-full` the default boot instrument, reboots, requires
loss/return of ICMP, then verifies a changed boot ID. Startup times include
shutdown; onboard timings use the monotonic clock. The onboard allowance is
at most 35 seconds, reduced when necessary to reserve time for host checks.

For iteration without reboot:

```sh
.venv/bin/python examples/red-pitaya/bist-full/client.py --host 192.168.1.85 \
  --no-reboot --analog-mask 1 --seed 0xb157f011 --output tmp/bist-full-results
```

Masks `0`, `1`, `2`, `3` declare neither cable, channel 0, channel 1, or both.
Each run checks an independent coverage manifest: missing cases, unexpected
skips, duplicated cases, absent identities and incomplete groups cannot pass.
The default fixture produces **135 onboard checks and 41 host checks**; many
rows aggregate multiple patterns or transactions. Expected faults pass only
when the corresponding detector fires, followed by verified recovery.

## New hardware

The design uses about one third of the Zynq-7010's LUTs and registers. It has:

- A new elastic 32-bit AXI Stream packet exerciser with an independent source
  and checker, final-byte `TKEEP`, deterministic input/output pauses and stable
  payloads under backpressure. Atomic counters track words, packets, corruption,
  malformed byte masks, packet length, sample loss and stalls.
- A 64-bit DDR scatter/gather DMA with both data realignment engines enabled.
  CPU and DMA share only a fixed 8 MiB region inside the boot's exclusive
  reservation. No transfer addresses arbitrary Linux memory.
- A 16 KiB dual-port BRAM; its first 1024 packed words drive independent DAC
  channels. ADC capture has programmable decimation and explicit overflow.
- An independent programmable level-interrupt source with generated-event
  counters, acknowledgement and snapshot handshakes.
- Two internal 460800-baud UARTs with 64-byte FIFOs and independent UIO IRQs,
  plus XADC temperature and six PS/PL supply measurements.

It reuses the compact instrument's UART RTL, UIO worker, measurement and
checksummed-checkpoint helpers. It does not reuse the compact FPGA datapath.

## Coverage

| Area | Checks |
| --- | --- |
| OS mappings | Actual UIO/WC backends; no-map boot reservation contains the entire DMA window; exclusion from Linux System RAM; 32 map/unmap lifetimes; a child process must fault on a real write through the read-only mapping |
| Memory | March C- over every word of 16 KiB BRAM and 8 MiB reserved DDR; 64 walking-one/zero trials; address aliasing; byte/halfword write strobes; address-dependent full-window patterns |
| Registers | 256 echo patterns; atomic snapshot acknowledgement across signed and unsigned token wrap; converter-clock sanity against monotonic time |
| DMA lengths | 28 lengths from one byte through 65537 bytes, around word, cache-line, page and descriptor boundaries; exact byte comparison, lengths, PL checks and 64-byte guards |
| DMA layout | Eight distinct TX/RX offsets including 4 KiB crossings; 2/3/31/32/63/64-descriptor chains; 16 buffer/descriptor reuse generations; independent TX-only checker and RX-only generator |
| Flow control | Deterministic 50% and 94% stream pauses; elastic VALID/data/TKEEP/TLAST behavior; 2 MiB duplex payload capacity measured separately from preparation and verification |
| DMA faults | Zero-length and already-completed descriptors; corrupted source data; output corruption after the checker; early/missing TLAST; masked UIO IRQ. Each is followed by a clean transfer and guard verification |
| Interrupts | 32 acknowledged/rearmed 1 kHz level events; pending level retention while UIO is masked; redelivery and clearing to a quiet timeout |
| UART | Ten full-duplex lengths around FIFO boundaries; 100 request/reply frames with whole-population latency; idle timeout; deliberate RX and TX overruns; reset and exact duplex recovery |
| Analog | Three amplitudes, three frequencies, positive/negative DC offsets and three decimations per connected channel; coherent amplitude, DC, residual and clipping; intentional unserviced-capture overflow and DMA recovery |
| Combined load | Shared start after initialization; CPU memory churn on CPU1, DMA on CPU0, real-time UART workers on CPU0/1; all digital data checked during concurrent load |
| SDK | Invalid control arguments; uint32/uint64 boundaries; exact IEEE754 signed-zero/infinity roundtrips; empty through 64 KiB vector payloads; a stalled partial-vector client must not lock out other calls; invalid IDs/partial-header EOF; 80 distinct-payload vector calls across four concurrent clients |
| Lifecycle | Exact report retention through service/overlay reload, followed by a fresh verified DMA transfer and duplex UART interrupt/data probes |

The additional destructive-test script operates only on its own new run:

```sh
.venv/bin/python examples/red-pitaya/bist-full/tests/hardware_checks.py \
  --host 192.168.1.85 --ssh-known-hosts path/to/known_hosts \
  --output tmp/bist-full-recovery
```

It exercises budget expiration, active-start rejection, cancellation with two
completion waiters, service stop and SIGKILL during combined traffic, truncated
checkpoint fallback, rejection of two corrupt generations, and restoration of
the original checkpoint files. It has a separate 60-second test deadline.

## Results and measurement policy

Reports contain every check's outcome, group, value/unit, elapsed time, detail
and relevant hardware counters. Metadata identifies instrument/SDK/OS image,
kernel, boot ID, FPGA DNA, FPGA/server/source SHA256 and workload parameters.
The host adds its own source SHA256, protocol and lifecycle results.

Results are persisted under `/var/lib/koheron/bist-full/<run-id>/`, with a
`latest` pointer. Bounded CRC-protected generations retain previous checkpoints;
fallback is explicitly unhealthy and an interrupted run cannot become complete.
Host results are stored in the same checkpoint after recovery. The host writes
`report.json`, `onboard.json` and `scorecard.txt` to its output directory before
and after service restart, preserving completed data if later checks fail.

The webpage at the board's root displays/downloads those results and can run
or cancel the onboard suite. CLI and webpage wait through a separate SDK driver,
without polling the measured driver. Report reads during an active run are
counted; unrelated applications and traffic remain operator-controlled.

Performance numbers are diagnostic measurements, without regression verdicts.
Saturation latency changes with achieved competing load. Use compact `bist`'s
controlled-load comparison for SDK latency comparisons. Analog values are
uncalibrated and are not metrology specifications. This fixture does not cover
the unconnected analog channel, external GPIO/auxiliary analog inputs, external
clocking, external UART wiring, Ethernet line-rate capacity, SD power-loss
durability, all DDR cells, or an extended thermal soak.

DMA alignment/descriptor semantics follow [AMD PG021](https://docs.amd.com/r/en-US/pg021_axi_dma/Scatter-Gather-Descriptor).
Sensor conversion and monitoring follow [AMD UG480](https://docs.amd.com/r/en-US/ug480_7Series_XADC/Sequencer-Modes).

## Local verification

```sh
make check CFG=examples/red-pitaya/bist-full/stemlab-125-14.mk
.venv/bin/python -m pytest --import-mode=importlib examples/red-pitaya/bist/tests \
  examples/red-pitaya/bist-full/tests -q
source /tools/Xilinx/2025.1/Vivado/settings64.sh
vivado -mode batch -source examples/red-pitaya/bist-full/tests/run_sim.tcl \
  -tclargs tmp/bist-full-sim
```

The RTL testbench checks payload stability while stalled, partial-byte tails,
independent generation, input corruption, invalid KEEP, early/missing packet
end, ADC overflow, reset during a stalled beat, level IRQ clear and token wrap.
Build/host checks and measured hardware results are reported separately in
[the validation record](VALIDATION.md).
