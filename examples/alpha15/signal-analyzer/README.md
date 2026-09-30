# ALPHA15 signal analyzer

Build and load the instrument from the SDK root:

```sh
make CFG=examples/alpha15/signal-analyzer/config.mk
make run CFG=examples/alpha15/signal-analyzer/config.mk HOST="$BOARD_IP"
```

The optional RF DAC0 test tone is off by default. It is a 7.324 kHz triangle
generated from the 240 MHz DAC clock. With DAC0 connected to ADC0, run the
headless loopback check:

```sh
PYTHONPATH=python:examples/alpha15/signal-analyzer/python \
  .venv/bin/python3 examples/alpha15/signal-analyzer/python/loopback_test.py "$BOARD_IP"
```

The check compares the low-frequency power spectrum with the tone off, on, and
off again. It always disables the tone before exiting. For an interactive plot
of the two decimated spectra, run `python/test_decimator.py` with the same
`PYTHONPATH` and `HOST`.
