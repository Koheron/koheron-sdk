# Hardware validation — 2026-10-16

Instrument **bist-full 0.2.0**, workload 2, original STEMlab 125-14 Gen1.
DAC0 → ADC0, channel 1 unconnected, LV input range. Board at 192.168.1.85;
DNA 23675955948361837; factory serial 160200540, revision 1.1.2.
OS image `20261015.195921.a8061a0f1cc6`, Ubuntu 26.04.1, kernel `6.18.0-xilinx`.
Reference host Ubuntu 24.04, Vivado/Vitis 2025.1, ARM GCC 15.

**Functional verdict: FAIL.** Four raw-loopback qualification checks fail
repeatably: high-frequency gain, high-frequency clipping, step ringing/settling
and broadband correlation. The aggregate host verdict also fails. DDR,
CPU/cache, storage, DMA, UART, interrupt, protocol and lifecycle checks pass.
The failure verdict is retained in the CLI, checkpoint and webpage.

## Build and host verification

- FPGA built with timing enforcement: **0.119 ns setup**, **0.020 ns hold**,
  no negative slack.
- **7846/17600 LUTs (44.58%)**, **11663/35200 registers (33.13%)**,
  **8/60 BRAM tiles (13.33%)**, **0/80 DSPs**.
- SDK production server/package build and deployment succeeded with warnings
  treated as errors.
- `make check CFG=examples/red-pitaya/bist-full/stemlab-125-14.mk`:
  **161 host checks passed**.
- Combined compact/full BIST pytest suite: **127 passed**. Includes native
  FFT known answers and constant/railed ADC evidence, without NaN artifacts.
- RTL simulation: elastic packet path and independent DDR master passed.
  Covers independently stalled AXI channels, 4 KiB splitting, 1/16/64/256-beat
  bursts, byte strobes, exact injected corruption, invalid request fences,
  BRESP/RRESP/early-RLAST errors, draining abort and subsequent recovery.

## Reboot-to-recovery measurements

Each run reboots, requires network disappearance/return and a changed boot ID,
executes the instrument with four concurrent paced SDK clients, downloads raw
captures, cross-checks spectra with NumPy, tests protocol corner cases,
reloads the service/overlay, checks exact report retention and executes fresh
DDR/DMA/UART traffic. The webpage remained connected and used quiet completion
waits. The end-to-end allowance remains **60 seconds**.

| Run | End-to-end | Onboard | Report reads while active | Pass / Fail / Skip |
| --- | ---: | ---: | ---: | --- |
| boot1 | 41.041 s | 14.276 s | 0 | 238 / 5 / 1 |
| boot2 | 38.320 s | 14.216 s | 0 | 238 / 5 / 1 |
| boot3 | 37.508 s | 14.268 s | 0 | 239 / 5 / 1 |

The final run has **197 onboard and 48 host checks**. The preceding two runs
have 47 host checks; the final client additionally verifies artifact access
validation. All runs have the same four analog failures and aggregate failure.
The sole skip is the absent channel-1 analog cable. External wall time for
boot3, including Python startup and report export, was **37.87 seconds**.

## New independent paths verified on hardware

- HP1 master wrote and checked all **128 MiB**, seven patterns, **1.75 GiB**
  total payload traffic. CPU independently checked every 4 KiB page; PL checked
  CPU-written data at the end of the reservation. A CPU-flipped bit produced
  exactly one mismatch at the expected address with exact expected/actual data.
- Hardware rejected out-of-window/misaligned/zero-length/illegal-burst requests
  with **zero AXI transactions**. Complementary byte writes preserved the
  unselected lanes. Throttled abort drained the current burst and recovered.
- An **8 MiB** pattern survived the CPU, media, DMA, UART and converter phases.
- Both CPUs passed executed arithmetic/CRC/VFP/NEON checks, peer reads over
  **32 MiB** cacheable RAM and **4096** release/acquire cache-line handoffs.
- Own **4 MiB** SD file passed fsync/rename/directory-fsync/O_DIRECT readback;
  actual changed-file-byte and truncation controls were rejected; files removed.
- **32** seeded asymmetric SG packets passed independent TX/RX segmentation,
  different DRE offsets, byte comparison, every descriptor and PL checks.
- Both UART receivers rejected a forced low stop bit, raised an error IRQ and
  enqueued no corrupt byte; subsequent traffic recovered.
- **Three seconds** of CPU churn, HP0 DMA, independent HP1 write/check traffic
  and **131072 bytes in each UART direction** completed without integrity errors.
  Four network clients simultaneously verified distinct SDK vector payloads.

## Analog findings and independent measurement evidence

Each connected channel retains **21 captures**, each containing generated DAC
codes and both 8192-sample ADC records. Both ADC channels and every DMA/sample
loss/guard diagnostic remain available after service reload. The client saves
these to `captures/`, and records NumPy/onboard FFT agreement in
`spectral-crosscheck.json`.

The following values are from boot3:

| Stimulus | Measured result | Qualification |
| --- | --- | --- |
| 1.58691 MHz, 4096 DAC codes | Gain 0.9527; SNDR 51.41 dB; 0 clipped samples | PASS |
| 4.51660 MHz, 4096 DAC codes | Gain 0.9892; SNDR 51.05 dB; 0 clipped samples | PASS |
| 15.99121 MHz, 4096 DAC codes | Gain 1.7726; SNDR 36.21 dB; 0 clipped samples | FAIL |
| 25.75684 MHz, 4096 DAC codes | Gain 0.0214; SNDR -9.17 dB; 7666 clipped samples | FAIL |
| 25.75684 MHz, 512 DAC codes | Gain 6.643; SNDR 42.95 dB; no clipped samples | Reduced-drive diagnostic PASS |
| Bipolar step | Overshoot 98.6%; settling 560 ns to 2% band | FAIL: limits 40%, 192 ns |
| Broadband sequence | Correlation 0.7910 | FAIL: minimum 0.9 |
| Seven DC levels | Gain 0.9011; offset -210.2 codes; max residual 10.24 codes | PASS |

No unintended FIFO sample loss, DMA errors or guard damage accompanied these
findings. NumPy independently reproduced onboard amplitude, SNDR, THD and SFDR.
The 15.99 MHz case fails its gain ceiling of 1.7, despite meeting its spectral
limits. At 25.76 MHz, reduced drive removes clipping and exposes substantial
frequency-dependent gain. Raw data confirms the ringing and correlation result.

These are qualifications of the **uncalibrated, unequalized DAC/cable/ADC
loopback**. The instrument bypasses Red Pitaya's frequency equalization;
[Red Pitaya documents that equalization compensates the analog front-end and
changes ringing, settling and frequency response](https://redpitaya.readthedocs.io/en/latest/appsFeatures/systemtool/calibration/frequency_calibration.html).
The results do not isolate the ADC, DAC, cabling, loading or compensation as the
cause. They do establish that the selected raw-loopback profile fails. Factory
EEPROM is read only; no calibration or network configuration was changed.

At session entry the already-running boot had MAC `ca:70:4c:f8:d6:37`, differing
from factory `00:26:32:f0:1c:6a`. All three new boots recovered the factory MAC
and passed the new comparison. The initial mismatch is retained as evidence;
these runs do not establish a persistent MAC failure.

## Cancellation, process death and persistence

The separate destructive-test script completed in **31.172 seconds**:

- Budget expiration produced a partial `timeout` report.
- Active-start rejection and cancellation with two completion waiters passed;
  cancellation completed in **26.6 ms**.
- Active service stop during independent DDR traffic completed in **218.6 ms**.
- SIGKILL during DDR traffic restored an `interrupted` report. Fresh independent
  DDR, DMA and UART traffic passed after reload.
- Truncating the current checkpoint selected the intact previous generation
  and explicitly marked storage unhealthy. Corrupting both generations produced
  `corrupt`, with no checks presented as completed.
- Restoring this script's own originals recovered every check and host fixture
  exactly. Functional analog failures remained visible throughout.

## Identity and evidence

Final onboard implementation:

- `source_sha256`: `b050f420df0bf5c85a9c5afee145a22299388cafaa16a3703e96e771880b4dbb`
- `fpga_sha256`: `3e9a2147f823f91b949eddd35fb2b601a5a021de19fe40e06b51afec557ec055`
- `server_sha256`: `3ed31960296cd06cbc04baf1fe2098fea003270cf500c37280dd87adea95efc2`
- Final host client SHA256: `a6538bc355181a209eb7b9f42c0254c3c63ef71627463997eb57ac62b87f48f0`

Local evidence: `tmp/bist-full-v2-evidence/` contains every run's report,
scorecard, raw captures, independent FFT comparisons, recovery reports,
pre-reboot identity evidence and the plotted loopback response. FPGA and host
check logs are retained in `tmp/bist-full-{fpga,sim,check,doctor}-v2.*`.

The 0.1.0 validation is preserved in Git history. It exercised a smaller set
of paths and did not characterize this high-frequency/transient response.
