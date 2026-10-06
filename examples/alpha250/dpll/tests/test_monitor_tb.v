`timescale 1 ns / 1 ps

module test_monitor_tb;
  localparam N = 256;
  reg clk = 0, vin_old = 0, vin_new = 0;
  reg [31:0] din_old = 0, din_new = 0;
  wire rin_old, rin_new, vout_old, vout_new;
  wire [31:0] dout_old, dout_new;
  reg [31:0] output_old [0:N-1], output_new [0:N-1];
  integer sent_old = 0, sent_new = 0, got_old = 0, got_new = 0, tick, i;
  always #2 clk = ~clk;
  monitor_wrapper dut (
    .clk(clk), .vin_old(vin_old), .vin_new(vin_new), .din_old(din_old), .din_new(din_new),
    .rin_old(rin_old), .rin_new(rin_new), .vout_old(vout_old), .vout_new(vout_new),
    .dout_old(dout_old), .dout_new(dout_new)
  );

  function [31:0] sample(input integer n);
    if (n < 32) sample = 0;
    else if (n < 64) sample = (n % 2) ? 32'h7fffffff : 32'h80000000;
    else if (n < 96) sample = (n % 4 == 0) ? 1 : 0;
    else if (n < 128) sample = (n % 4 == 0) ? -1 : 0;
    else sample = (32'h9e3779b9 * n) ^ (32'h7f4a7c15 * (n + 17));
  endfunction

  always @(posedge clk) begin
    if (vin_old && rin_old !== 1'b1) $fatal(1, "Reference FIR stalled");
    if (vin_new && rin_new !== 1'b1) $fatal(1, "FIR cannot sustain the minimum CIC rate of four");
    if (vout_old) begin
      if ((^dout_old) === 1'bx || got_old >= N) $fatal(1, "Invalid reference output");
      output_old[got_old] = dout_old;
      got_old = got_old + 1;
    end
    if (vout_new) begin
      if ((^dout_new) === 1'bx || got_new >= N) $fatal(1, "Invalid fast output");
      output_new[got_new] = dout_new;
      got_new = got_new + 1;
    end
  end

  initial begin
    repeat (64) @(negedge clk);
    for (tick = 0; tick < N * 20 + 256; tick = tick + 1) begin
      @(negedge clk);
      vin_old = (tick % 20 == 0 && sent_old < N);
      vin_new = (tick % 4 == 0 && sent_new < N);
      if (vin_old) begin din_old = sample(sent_old); sent_old = sent_old + 1; end
      if (vin_new) begin din_new = sample(sent_new); sent_new = sent_new + 1; end
    end
    if (got_old != N / 2 || got_new != N / 2)
      $fatal(1, "FIR dropped samples: reference=%0d fast=%0d expected=%0d", got_old, got_new, N / 2);
    // The original symmetric architecture prepends two zero coefficients:
    // one extra decimated output sample. The new architecture needs no padding.
    if (output_old[0] !== 0) $fatal(1, "Unexpected original FIR padding");
    for (i = 0; i < got_new - 1; i = i + 1)
      if (output_old[i+1] !== output_new[i])
        $fatal(1, "FIR arithmetic mismatch at output %0d: %h/%h", i, output_old[i+1], output_new[i]);
    $display("Monitor checks passed: %0d exact outputs, 250 MHz clock, sustained input every four clocks", got_new - 1);
    $finish;
  end
endmodule
