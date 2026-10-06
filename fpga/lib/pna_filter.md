# Shared PNA split CIC

`fpga/lib/pna_filter.tcl` builds the same filter for DPLL, ALPHA250 PNA,
ALPHA250-4 PNA and Red Pitaya PNA:

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
resets every history. The controller holds the rate word stable during its
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

## Build validation — 2026-10-06

Vivado 2025.1 full instrument implementations pass the SDK's strict setup,
hold, pulse-width and bus-skew gates. ARM servers, device-tree overlays,
web bundles and instrument ZIP packages also build successfully.

| Instrument | ADC clock | Programmable filter clock | Setup slack (ns) | Hold slack (ns) | Bus-skew constraints checked |
| --- | ---: | ---: | ---: | ---: | ---: |
| ALPHA250 DPLL | 250 MHz | 143 MHz | 0.035455 | 0.039732 | 9 |
| ALPHA250 PNA | 250 MHz | 143 MHz | 0.029770 | 0.012405 | 11 |
| ALPHA250-4 PNA | 250 MHz | 143 MHz | 0.021506 | 0.041813 | 12 |
| Red Pitaya PNA | 125 MHz | 125 MHz | 0.186185 | 0.019563 | 16 |

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

**Hardware validation:** the new packages have not been deployed or tested
on a board. These results establish build, simulation and software behavior;
they do not establish measured phase noise, sustained CPU coverage, converter
latency or loop stability. Existing PNA hardware measurements describe the
previous 200 MHz design. Use a V1 OS image with these instruments.
