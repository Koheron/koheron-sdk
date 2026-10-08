# ALPHA250-4 FFT

Select this instrument with `CFG=examples/alpha250-4/fft/config.mk`.
V1 instruments require a V1 OS image; the image supplied with the board is V0.

```sh
make validate CFG=examples/alpha250-4/fft/config.mk
make web CFG=examples/alpha250-4/fft/config.mk
make -j CFG=examples/alpha250-4/fft/config.mk
make CFG=examples/alpha250-4/fft/config.mk HOST=192.168.1.100 run
```

`run` streams logs. Ctrl+C stops the stream and leaves the instrument running.

The interface uses the same FFT workspace as ALPHA250, ALPHA15 and Red Pitaya:
pause/resume, visible-range peak readout, references, one-second linear PSD
averaging, max hold, spectrogram, occurrence density, CSV and PNG exports.
There is no RF DAC generator on ALPHA250-4. Precision DACs use the shared digit
editor: type and press Enter, or select a digit and tune with arrows or the
mouse wheel. Startup reads settings without writing outputs. Board telemetry
runs once per second and stops on exit.

The four input buttons map to the existing two-engine acquisition protocol:
ADC 0/1 select channels 0/1 on the first ADC pair; ADC 2/3 select channels 0/1
on the second pair. The channel selection applies to both engines in hardware;
the displayed pair is selected locally. The adapter retains the existing
`ddIdd` control tuple and reads the window and reference separately. Bins use
`k * fs / N`, with DC at zero, using the selected pair's sample rate. dBm
conversion also uses that sample rate and the returned window coefficients.

Settings changes suspend spectrum reads until controls are read back; replies
already in flight are discarded. Reference frames keep their original channel,
sample rate and window metadata. This is a web-interface update: server and
FPGA acquisition remain unchanged, including their existing accumulator timing
and buffering. It does not introduce acquisition-generation tagging or guarantee
that the first server buffer after a settings change contains only new settings.

## Host checks

```sh
bash web/tests/run.sh fft
```

`tests/test_workspace.cjs` mounts the real templates, decoder and shared
workspace with simulated board I/O. It checks startup, four-input command and
engine mapping, selected-pair frequency/voltage conversions, binary tuple
layout, settings changes, in-flight replies, references, pause, history,
CSV/PNG exports, precision readbacks, connection failure and teardown.

Host checks and web builds do not validate numerical calibration, acquisition
on a board or FPGA timing; those need separate hardware/build validation.
