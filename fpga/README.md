# FPGA builds

From the SDK root, select an instrument for your board:

```sh
make CFG=examples/red-pitaya/fft/stemlab-125-14.mk ENFORCE_TIMING=1 fpga
make CFG=examples/red-pitaya/fft/stemlab-125-14.mk timing
```

`fpga` builds the bitstream. `ENFORCE_TIMING=1` fails the build on routed timing
violations; `timing` checks the result separately.
