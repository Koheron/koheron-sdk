# FPGA builds

From the SDK root, select an instrument for your board:

```sh
make CFG=examples/red-pitaya/fft/config.mk ENFORCE_TIMING=1 fpga
make CFG=examples/red-pitaya/fft/config.mk timing
```

`fpga` builds the bitstream. `ENFORCE_TIMING=1` fails the build on routed timing
violations; `timing` checks the result separately.
