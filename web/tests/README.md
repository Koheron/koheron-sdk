# Browser regressions

Run shared clock/plot checks and the selected interface suites from the SDK root:

```sh
bash web/tests/run.sh fft
bash web/tests/run.sh alpha15
bash web/tests/run.sh phase-modulator
bash web/tests/run.sh pna
```

The default is `all`. `alpha250`, `alpha250-4` and `dpll` select a PNA/DPLL suite.
Shared clock, plot and precision-DAC checks run once. `alpha15` runs these and
the ALPHA15 signal-analyzer workspace suite. FFT checks cover the protocol, controls,
plot, scheduling, references, CSV/PNG/history exports, generator integration,
precision DACs and ALPHA15 workspace. The standalone generator suite also checks
the shared digit editor's asynchronous tuning and disposal.

PNA/DPLL wire-format integration requires the spectrum fixture produced by the
host runner's `cpp` stage. FFT and generator browser suites need no prior build.
The complete host runner also checks compiled ALPHA250-4 payloads:

```sh
make CFG=examples/alpha250-4/phase-noise-analyzer/config.mk web
PNA_TEST_MODE=docker bash server/drivers/phase-noise/tests/run-host.sh
```

Run `npm install --prefix web --no-package-lock` for native dependencies. The
shared environment resolves `NODE_PATH`, local packages or Docker's packages;
fallback installs are cached under `tmp/tests/web-deps`. Direct instrument
runners in `web/fft/tests/` and `web/phase-noise/tests/` use the same environment.
