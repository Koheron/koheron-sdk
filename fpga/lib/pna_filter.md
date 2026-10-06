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
