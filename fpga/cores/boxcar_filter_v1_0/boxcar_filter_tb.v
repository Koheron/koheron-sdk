`timescale 1 ns / 1 ps

module boxcar_filter_checker #(parameter DATA_WIDTH = 16)(output reg done = 0);
  reg clk = 0;
  reg signed [DATA_WIDTH-1:0] din = 0;
  wire signed [DATA_WIDTH-1:0] legacy, fast;
  reg signed [63:0] history [0:5];
  reg signed [63:0] expected_legacy, expected_fast;
  integer cycles = 0, i, j, seed = 81723;
  localparam signed [DATA_WIDTH-1:0] MAX_VALUE = {1'b0, {(DATA_WIDTH-1){1'b1}}};
  localparam signed [DATA_WIDTH-1:0] MIN_VALUE = {1'b1, {(DATA_WIDTH-1){1'b0}}};

  always #2.5 clk = ~clk;
  // The default instance protects the latency used by other instruments.
  boxcar_filter #(.DATA_WIDTH(DATA_WIDTH)) old_filter(clk, din, legacy);
  boxcar_filter #(.DATA_WIDTH(DATA_WIDTH), .LOW_LATENCY(1)) fast_filter(clk, din, fast);

  always @(posedge clk) begin
    for (j = 5; j > 0; j = j - 1) history[j] = history[j-1];
    history[0] = $signed(din);
    cycles = cycles + 1;
    // Independent four-sample averages, including floor rounding for negatives.
    expected_legacy = (history[2] + history[3] + history[4] + history[5]) >>> 2;
    expected_fast = (history[1] + history[2] + history[3] + history[4]) >>> 2;
    #0.1;
    if (cycles > 6) begin
      if ($signed(legacy) !== expected_legacy)
        $fatal(1, "Legacy boxcar width=%0d cycle=%0d expected=%0d got=%0d", DATA_WIDTH, cycles, expected_legacy, legacy);
      if ($signed(fast) !== expected_fast)
        $fatal(1, "Fast boxcar width=%0d cycle=%0d expected=%0d got=%0d", DATA_WIDTH, cycles, expected_fast, fast);
    end
  end

  initial begin
    for (i = 0; i < 10000; i = i + 1) begin
      @(negedge clk);
      if (i < 32) din = 0;
      else if (i < 64) din = MAX_VALUE;
      else if (i < 96) din = MIN_VALUE;
      else if (i < 160) din = (i % 2) ? MAX_VALUE : MIN_VALUE;
      else if (i < 224) din = (i % 4 == 0) ? 1 : 0;
      else if (i < 288) din = (i % 4 == 0) ? -1 : 0;
      else if (i < 320) din = -1;
      else din = $random(seed);
    end
    repeat (8) @(negedge clk);
    done = 1;
  end
endmodule

module boxcar_filter_tb;
  wire done8, done16, done32;
  boxcar_filter_checker #(8) check8(done8);
  boxcar_filter_checker #(16) check16(done16);
  boxcar_filter_checker #(32) check32(done32);
  initial begin
    wait (done8 && done16 && done32);
    $display("Boxcar checks passed: 8/16/32-bit arithmetic, legacy latency and one-cycle reduction");
    $finish;
  end
endmodule
