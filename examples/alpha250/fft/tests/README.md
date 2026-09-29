# Host regression tests

From the repository root, with the SDK Python dependencies installed:

```sh
PYTHONPATH=python python3 examples/alpha250/fft/tests/test_protocol.py
node examples/alpha250/fft/tests/test_web_protocol.js
```

The web test uses the repository's `typescript` dependency. Both protocol tests
exercise the actual client decoder with an eight-field server response, including
internal and external reference selections. The Python test also checks that the
following response remains aligned.

After generating the FFT server headers (`make CFG=examples/alpha250/fft/config.mk
server drivers_json`), check the owned-snapshot API contract:

```sh
g++ -std=c++20 -fsyntax-only -DKOHERON_SERVER_BUILD \
    -I. -Iserver/external_libs -Itmp/examples/alpha250/fft/server \
    examples/alpha250/fft/tests/test_snapshot.cpp
```

These tests do not require or control a board. They do not validate FPGA timing,
FFT numerical accuracy, or acquisition boundaries after a settings change.

Spectrum display regression:

```sh
node examples/alpha250/fft/tests/test_web_plot.js
```

Checks zero-based bin frequencies (including the 40.008544921875 MHz bin), exact
spectrum length, zero-DC peak handling, pause/resume, displayed-frame metadata,
and cursor interpolation when a shared plot uses decimation.
