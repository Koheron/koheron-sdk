# Historical DPLL regression references

These implementations are retained for numerical and latency comparisons:

- `cordic.tcl`: historical boxcar detector with 16-bit vendor and 24-bit custom
  phase extraction.
- `gain_multiplier.v` and `corrector.tcl`: the original integer-gain controller.
- `test_detector*`, `test_corrector*` and `test_gain_tb.v`: their regression fixtures.

Run them with packaged DPLL cores and Vivado available:

```sh
DPLL_VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh \
    bash examples/alpha250/dpll/tests/reference/run.sh
```

`tests/run-fpga.sh` includes these checks. Current instrument wiring uses
`tcl/split_detector.tcl` and `tcl/corrector.tcl`; production checks are in
`tests/check_phase_feedback.tcl`, `tests/test_p_frontend.tcl` and the full-project
design/timing checks. Keep historical benchmarks separate from current
instrument latency and timing claims.
