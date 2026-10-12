# AXI status register

This read-only AXI interface exposes `sts_data` as consecutive words of
`AXI_DATA_WIDTH` bits. Byte addresses select words; the low byte-offset bits are
ignored. Addresses beyond `STS_DATA_WIDTH / AXI_DATA_WIDTH` words return zero
with an OKAY response. Writes receive an OKAY response without changing status.

After an address is accepted on clock edge N, the word index and full-address
bounds check are registered on N+1. The selected live status word is sampled and
`RVALID` is asserted on N+2. This adds exactly one clock cycle to the previous
read latency and separates the bounds check from the status data multiplexer.
The response remains stable until accepted. Only one read is outstanding at a
time, and reset discards a pending transaction.

Run the AXI handshake, reset, address and sampling regressions with:

```sh
VIVADO_SETTINGS=/tools/Xilinx/2025.1/Vivado/settings64.sh \
  bash fpga/tests/axi_sts_register/run.sh
```
