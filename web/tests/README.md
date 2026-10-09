# Browser regressions

Run shared clock/plot checks and the selected interface suites from the SDK root:

```sh
bash web/tests/run.sh fft
bash web/tests/run.sh alpha15
bash web/tests/run.sh phase-modulator
bash web/tests/run.sh pna
```

The default is `all`. `alpha250`, `alpha250-4` and `dpll` select a PNA/DPLL suite.
Shared DDS digit-editor, clock, plot, precision-DAC, telemetry adapter and
polling checks run once. `alpha15` adds the ALPHA15 workspace suite. FFT checks cover the protocol, controls,
plot, scheduling, references, CSV/PNG/history exports, generator integration,
precision DACs and ALPHA15/ALPHA250-4 workspaces. The standalone generator suite also checks
the shared digit editor's asynchronous tuning and disposal.

PNA/DPLL wire-format integration requires the spectrum fixture produced by the
host runner's `cpp` stage. FFT and generator browser suites need no prior build.
The complete host runner also checks compiled ALPHA250-4 payloads:

```sh
make CFG=examples/alpha250-4/phase-noise-analyzer/config.mk web
PNA_TEST_MODE=docker bash server/drivers/phase-noise/tests/run-host.sh
```

Run `npm ci --prefix web` for native dependencies. The
shared environment resolves `NODE_PATH`, local packages or Docker's packages;
fallback installs are cached under `tmp/tests/web-deps`. Direct instrument
runners in `web/fft/tests/` and `web/phase-noise/tests/` use the same environment.

The builder uses Node 24 LTS; native tests require Node 24.15 or newer within
the Node 24 series. Both npm dependency sets have committed lockfiles. Rebuild
the builder after changing either lockfile or the compiler runner/emitter:

```sh
docker build -f web/Dockerfile.web -t koheron-web:node24 web
docker run --rm -v "$PWD:/work" -w /work koheron-web:node24 bash web/tests/run.sh all
```

TypeScript 7 checks each instrument's complete global program. The shared
`web/build.cjs` runner then uses esbuild to emit an ES2020 script in the source
order selected by `WEB_FILES`, preserving the existing `app.js` and dashboard
script entry points. Native builds can set `WEB_COMPILE='node web/build.cjs'`.
Browser fixtures use the same emitter, without depending on TypeScript's
unstable compiler API. Both tools are pinned in the main web lockfile.

The dashboard and instruments share checksum-verified jQuery 3.7.1 and
Bootstrap 3.4.1. Bootstrap 3 rejects jQuery 4; those major upgrades require a UI
migration.

The owned Flot stack also has real Canvas/DOM checks (the other suites use
jsdom and transport fixtures):

```sh
npm ci --prefix web/plotting
npm test --prefix web/plotting
npm run benchmark --prefix web/plotting -- --frames=180 --rounds=3
```

To exercise the shipped UI libraries in real Chrome, first build the assets:

```sh
make CFG=examples/alpha250/fft/config.mk web www
make CFG=examples/alpha15/signal-analyzer/config.mk web
make CFG=examples/alpha250-4/phase-noise-analyzer/config.mk web
NODE_PATH="$PWD/web/node_modules" node --test web/tests/test_ui_assets.cjs
PLOT_JQUERY="$PWD/tmp/www/jquery.min.js" NODE_PATH="$PWD/web/node_modules" npm test --prefix web/plotting
```

`PLOT_JQUERY` selects the shipped jQuery for compatibility tests. Without it,
plot tests and benchmarks retain the frozen baseline dependency for comparisons.

See [plotting maintenance](../plotting/README.md) for browser installation,
source provenance, baseline conditions and profiling commands.

The standalone web-build regression checks that switching to older shared
assets refreshes existing output names, replaces removed TypeScript sources
and leaves unchanged builds up to date:

```sh
python3 web/tests/test_build.py
```
