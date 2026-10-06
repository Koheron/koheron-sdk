# Shared PNA split CIC

`fpga/lib/pna_filter.tcl` builds the split filter for the 250 MHz instruments:
DPLL, ALPHA250 PNA and ALPHA250-4 PNA:

```
32-bit phase at ADC clock
  -> six-stage fixed /2 (38 bits, packed in a 40-bit AXIS word)
  -> asynchronous AXIS FIFO
  -> six-stage programmable /(R/2) at FCLK1
  -> normalized 40-bit result
  -> existing PNA compensation FIR /2
```

Both CICs must have the same stage count, six, and differential delay one.
Their input-rate transfer functions multiply to

```
[(1-z^-2)/(1-z^-1)]^6 * [(1-z^-R)/(1-z^-2)]^6
    = [(1-z^-R)/(1-z^-1)]^6.
```

Thus the cascade preserves the original total-rate CIC response and DC gain.
The fast stage implements `(1+z^-1)^6` as pipelined two-sample sums; it retains
all six growth bits. The slow stage retains 110 bits in every integrator and
comb. Modular overflow cancels through the combs, and no interstage precision
is discarded. Pipeline registers add fixed delay, which does not change the
phase-noise PSD response.

Configure with the **total** even rate R, 4 through 8192. Each acquisition epoch
resets every history. The controller drives `filter_resetn` from an ADC-clock
register: combinational decoding of its binary state can create a brief reset
pulse during PRIME-to-STREAM and asynchronously reset the destination filter.
The controller holds the rate word stable during its
32-ADC-clock reset interval; `phase_stream_cdc` synchronizes the word and reset.
The slow CIC latches the rate once after reset. Live rate changes require a new
epoch. The fixed stage reports input readiness only after its downstream FIFO
leaves reset. Live ADC instants cannot be replayed: a later stall becomes sticky
sample-gap metadata and software rejects the damaged acquisition.

Normalization shifts by `ceil(log2(R^6))-8`, producing the same scale expected
by `phase_calibration::cic_correction`. The elaborated ROM computes this with
exact integer arithmetic; two registered shift stages separate normalization
from integrator and comb additions. Existing FIR coefficients, output precision
selection and software calibration remain applicable.

The DPLL `test_split_cic.tcl` simulation compares 40-bit output against an
independent Python integer reference at rates 4, 6, 8, 10, 14, 20, 32, 126,
128, 130, 8190 and 8192. Inputs exercise both signs, signed full scale and
internal modular overflow. The test also stalls the output and resets between
rates. `test_monitor_stream.tcl` checks sustained 250/143 MHz acquisition with
the production AXIS converter and FIR, plus gap detection and epoch recovery.
Full routed builds and board measurements are separate checks.

Red Pitaya instead uses `pna_create_unsplit_filter`: one programmable
six-stage Xilinx CIC, followed by the shared compensation FIR and quantizer,
all on its 125 MHz ADC clock. Its existing DMA FIFO crosses the filtered
stream to FCLK1. It accepts every integer CIC rate from 4 through 8192.
Both topologies use the same packet format, calibration and cyclic DMA ring.
The split topology's clock relaxation is needed by the 250 MHz instruments;
Red Pitaya retains its original filter clock period without an input crossing.

## Initial build validation — 2026-10-06

The following results precede the registered-reset correction described below.

Vivado 2025.1 full instrument implementations pass the SDK's strict setup,
hold, pulse-width and bus-skew gates. ARM servers, device-tree overlays,
web bundles and instrument ZIP packages also build successfully.

| Instrument | ADC clock | Programmable filter clock | Setup slack (ns) | Hold slack (ns) | Bus-skew constraints checked |
| --- | ---: | ---: | ---: | ---: | ---: |
| ALPHA250 DPLL | 250 MHz | 143 MHz | 0.035455 | 0.039732 | 9 |
| ALPHA250 PNA | 250 MHz | 143 MHz | 0.029770 | 0.012405 | 11 |
| ALPHA250-4 PNA | 250 MHz | 143 MHz | 0.021506 | 0.041813 | 12 |
| Red Pitaya PNA (superseded split build) | 125 MHz | 125 MHz | 0.186185 | 0.019563 | 16 |
| Red Pitaya PNA 1.3.1 (single CIC) | 125 MHz | 125 MHz | 0.284088 | 0.008955 | 15 |

The 143 MHz clock is the Zynq's actual 142.857 MHz FCLK1. The DPLL's final
physical implementation reuses its unchanged synthesized/placed full-design
checkpoint while rerunning physical optimization, routing and strict timing
checks with the final gain-control replication hook. No feedback pipeline
stages or timing exceptions are added.

Reproduce from a clean checkout with `make -j2 N_CPUS=4 CFG=.../config.mk all`
for each of the four instruments. The shared filter simulations, production
DSP scaler simulation, paired stream/reset tests and DPLL feedback/AXI
simulations pass. PNA and DPLL host/GUI suites pass, including sanitizers and
saved-rate migration. Chrome exercises the built DPLL UI with simulated
transport, including channel changes, reference traces, CSV/PNG exports,
coverage/queue status and wide/narrow layouts.

**Hardware validation:** the split Red Pitaya build passed timing but failed
settings-change tests on `192.168.1.84`: 24 trials across both inputs and CIC
10, 20, 50 and 80 recorded 447 hardware sample gaps. The previous FPGA
recorded zero gaps in the same trials, both with the previous server and with
the new streaming server. Red Pitaya therefore uses the single CIC again.
The rebuilt 1.3.1 single-CIC image passed 36 two-second trials across both
inputs and rates 4, 10, 20, 50, 67 and 80, accepting 5137 captures with zero
hardware gaps, DMA errors or overflows. A separate one-minute run at the
restored CIC 43 and +8-bit precision also recorded none. See the
[Red Pitaya hardware validation](../../examples/red-pitaya/phase-noise-analyzer/tests/hardware-validation.md#single-cic-restored-in-131-2026-10-06).
ALPHA250 package hardware validation remains pending. Build and simulation
results do not establish measured phase noise, sustained CPU coverage or loop
stability. Use a V1 OS image with these instruments.

## Final registered-reset build checks — 2026-10-06

Full production builds were repeated after registering the reset in both
controllers. The earlier DPLL timing pass above does not qualify this revision.

| Instrument | Setup slack (ns) | Hold slack (ns) | Result |
| --- | ---: | ---: | --- |
| Red Pitaya PNA (single CIC) | 0.178767 | 0.013317 | Full package and strict timing gates pass |
| ALPHA250 PNA | 0.039072 | 0.040046 | Full package and strict timing gates pass |
| ALPHA250-4 PNA | 0.103758 | 0.025333 | Full package and strict timing gates pass |
| ALPHA250 DPLL | -0.031810 | 0.042 | Setup gate fails; package not qualified |

The DPLL failure has 18 setup endpoints in the existing gain-programming
command-enable path, from `pending_reg[24]` to command-register clock enables.
Server, web, overlay and synthesis checks complete, but routed timing closure
remains a merge blocker. No timing gate or feedback latency was changed to
accept this result. The three PNA builds also pass pulse-width and their
15, 11 and 12 bus-skew checks respectively.

**Hardware tests:** the final Red Pitaya production image passed 36 restart
trials across both inputs and CIC 4, 10, 20, 50, 67 and 80, accepting 5062
captures with zero new hardware gaps, DMA errors or overflows. A separate
60-second CIC 50 run accepted 4581 captures with the same zero-error result.
ALPHA250 and ALPHA250-4 hardware validation remains pending. See the
[final production validation](../../examples/red-pitaya/phase-noise-analyzer/tests/hardware-validation.md#final-production-image-with-registered-reset-2026-10-06)
and the reset-path investigation immediately above it for the measured cause
of the original split-design gaps.
