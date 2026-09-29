# PSD counter and accumulator regression

After sourcing Vivado's `settings64.sh`, run from the SDK root:

```sh
vivado -mode batch -source fpga/tests/psd_accumulator/test.tcl \
    -tclargs tmp/tests/psd_accumulator
```

This generates the actual `bram_accumulator.tcl` design with Vivado's BRAM,
shift-register and floating-point IP simulation models. It checks all 192 output
bins of three successive averages of three 64-bin frames. Input values differ
by bin and frame, detecting shifted bins and contamination between averages.
The test fails Vivado's Tcl command if an assertion fails.

This test checks continuous acquisition. The accumulator's handling of input
pauses and draining at the end of a finite stream is a separate known issue.

The standalone `fpga/cores/psd_counter_v1_0/psd_counter_tb.v` also checks valid
pauses, single-sample frames, non-power-of-two frame/average lengths, 8192-bin
frames, and 1023-frame averages. Its checks are independent of the vendor IP.
