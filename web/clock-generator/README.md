# Clock controls

| Include | Provides |
| --- | --- |
| `driver.mk` | `ClockGenerator` RPC adapter; callback/Promise reference reads, sample-clock selection and DAC-rate reads |
| `components.mk` | Adapter and `ClockGeneratorApp` bindings |
| `reference-clock.mk` | Labelled 10 MHz selector: internal = 2, external = 0 |
| `sampling-frequency.mk` | ALPHA250/ALPHA250-4 200/250 MHz selector |

Construction sends no commands. `ClockGeneratorApp` accepts an optional third
argument called before a clock write to invalidate acquisition. Call `dispose()`
on exit; listener cleanup uses [InstrumentEvents](../instrument/README.md).

ALPHA15, ALPHA250/ALPHA250-4 FFT and ALPHA PNAs share the bindings. DPLL uses the
adapter and reference template with its own asynchronous lifecycle. ALPHA15
keeps its fixed 15 MS/s acquisition rate.

`tests/test_clock.cjs` checks RPC contracts, templates, notification order and
teardown through the [browser runner](../tests/README.md).
