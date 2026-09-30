These regressions run without a board. They exercise the production averager,
DMA reader and DDS driver, using an in-memory hardware backend for DMA and DDS.
The C++ tests run with AddressSanitizer and UndefinedBehaviorSanitizer. The web
test decodes payloads emitted by the production C++ serializer through the
compiled TypeScript driver.

Build the web assets, then run from this repository:

```sh
make web CFG=examples/alpha250-4/phase-noise-analyzer/config.mk
examples/alpha250-4/phase-noise-analyzer/tests/run.sh
```

The runner expects `.venv/bin/python3` with NumPy, SciPy and the Python client
requirements, plus the `cross-armhf:24.04` and `koheron-web:node20` Docker images.
`PNA_PYTHON`, `PNA_CPP_IMAGE` and `PNA_WEB_IMAGE` can override these defaults. The
C++ image must also provide native `g++-13`, since these tests run on the host CPU.

Coverage includes:

- Averaging-window growth, shrinkage and clear operations against a deque oracle.
- Fresh, disjoint acquisition windows and detection of overwritten ring data.
- Actual DMA copying across the ring boundary, synchronized X/Y data, cancellation, and configuration between complete transfer pairs.
- Fractional phase scaling above and below unity, including sub-hertz carrier offsets.
- Phase validation accepts steady frequency offsets and rejects impulses/discontinuities.
- Shared DMA API compatibility, successful completion, timeout and error status.
- DDS frequency precision, concurrent reads/writes, and nonfinite input rejection.
- Python command dispatch, cross-correlation selection, complete phase arrays and frequency axes.
- Web measurement and tracking decoders against C++ serialized quantities.

Hardware checks still needed: noise-floor calibration, FIFO behavior during live
CIC/LO changes, and tracking stability with equal and unequal carrier frequencies.
