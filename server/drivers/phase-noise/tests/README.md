# PNA and DPLL host regressions

The shared runner checks phase-noise processing, acquisition, Python clients and
browser controls without accessing a board or starting Vivado. Its default `all`
suite also runs the FFT and standalone generator browser regressions:

```sh
make CFG=examples/alpha250-4/phase-noise-analyzer/config.mk web
PNA_TEST_MODE=docker bash server/drivers/phase-noise/tests/run-host.sh
```

Choose `all` (default), `alpha250`, `alpha250-4` or `dpll` as the first argument.
The optional second argument selects `python`, `cpp`, `web` or `all` (default).
The `cpp` stage includes the independent SciPy numerical audits. The `all` stage
also decodes ALPHA250-4 C++ payloads through its compiled web bundle, so build that
bundle first when selecting ALPHA250-4 or all instruments. Browser checks load
the real sources and templates directly and need no web build, but their wire
format integration fixture requires the `cpp` stage to have run first.

For native execution, install g++-13, Eigen, the Python client dependencies,
NumPy and SciPy, and run `npm install --prefix web --no-package-lock`. Then use:

```sh
PNA_TEST_MODE=native bash server/drivers/phase-noise/tests/run-host.sh all
bash server/drivers/phase-noise/tests/run-host.sh dpll web
```

The shared runner defaults to native execution. The existing ALPHA250 and
ALPHA250-4 `tests/run.sh` commands retain their Docker default and delegate here.
Docker execution uses `cross-armhf:24.04` for native C++ tests and
`koheron-web:node20` for browser tests. Python runs on the host, selecting the SDK
virtual environment when available and otherwise `python3`.

Overrides are `PNA_PYTHON`, `PNA_TEST_CXX` (or `CXX`), `PNA_TEST_CXXFLAGS`,
`PNA_CPP_IMAGE`, `PNA_WEB_IMAGE`, `NODE_PATH` and `PNA_WEB_BUNDLE`. Extra compiler
flags are whitespace-separated. C++ tests use address/undefined-behavior
sanitizers and frame pointers; non-PIE executables avoid sanitizer address-space
collisions. Output and fallback browser dependencies are under `tmp/tests`.

Shared publication, cyclic DMA and streaming Welch tests run once per invocation.
Instrument C++ fixtures stay with their examples. The workflow in
`.github/workflows/ci.yml` compiles eleven web applications (PNA, DPLL, FFT,
ALPHA15, phase modulator, dual DDS and pulse generator) and runs the complete host suite natively.
CI also runs ALPHA15's existing C++ acquisition regression with sanitizers.
`bash web/tests/run.sh fft` runs just the shared/FFT browser checks, without
requiring PNA C++ payloads.

FPGA simulation and routed timing remain separate checks using each instrument's
FPGA runners. The DPLL `tests/reference/` directory contains historical detector
and gain implementations used as independent regression references. Its
production feedback wiring test uses `split_detector::create`, including the
shared 64-bit monitor history and the 40-bit/25-bit controller interface.

Host and FPGA simulation checks do not establish hardware lock, analog noise,
loop stability or converter latency.
