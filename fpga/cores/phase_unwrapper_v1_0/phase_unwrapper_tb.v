`timescale 1 ns / 1 ps

module phase_unwrapper_tb;
  localparam DIN_WIDTH = 8;
  localparam DOUT_WIDTH = 10;
  reg clk = 0, acc_on = 0, rst = 1;
  reg signed [DIN_WIDTH-1:0] phase_in = 0;
  wire signed [DIN_WIDTH:0] freq_out;
  wire signed [DOUT_WIDTH-1:0] phase_out;
  wire overflow;
  always #4 clk = ~clk;

  phase_unwrapper #(.DIN_WIDTH(DIN_WIDTH), .DOUT_WIDTH(DOUT_WIDTH)) DUT (
    .clk(clk), .acc_on(acc_on), .rst(rst), .phase_in(phase_in),
    .freq_out(freq_out), .phase_out(phase_out), .overflow(overflow)
  );

  integer expected_phase = 0, next_value;
  reg expected_overflow = 0;
  always @(posedge clk) begin
    if (rst) begin
      expected_phase = 0;
      expected_overflow = 0;
    end else if (acc_on) begin
      next_value = expected_phase + $signed(freq_out);
      if (next_value > 511 || next_value < -512) expected_overflow = 1;
      if (next_value > 511) next_value = next_value - 1024;
      if (next_value < -512) next_value = next_value + 1024;
      expected_phase = next_value;
    end
    #1;
    if ($signed(phase_out) !== expected_phase || overflow !== expected_overflow)
      $fatal(1, "wrong accumulated phase or sticky overflow");
  end

  task ramp(input integer step);
    integer i, value;
    begin
      value = 0;
      acc_on = 0;
      for (i = 0; i < 340; i = i + 1) begin
        @(negedge clk);
        value = value + step;
        if (value > 31) value = value - 64;
        if (value < -32) value = value + 64;
        phase_in = value;
        if (i == 4) acc_on = 1;
        @(posedge clk); #2;
        if (i > 4 && $signed(freq_out) != step)
          $fatal(1, "wrong frequency across a wrapped phase boundary");
      end
      if (!overflow) $fatal(1, "overflow was not detected");
      @(negedge clk); acc_on = 0;
      repeat (6) @(negedge clk);
      if (!overflow) $fatal(1, "overflow did not remain sticky while paused");
      rst = 1;
      repeat (4) @(negedge clk);
      if (overflow || phase_out != 0) $fatal(1, "reset did not clear overflow/phase");
      rst = 0;
    end
  endtask

  initial begin
    repeat (4) @(negedge clk);
    rst = 0;
    ramp(5);
    ramp(-5);
    $display("Phase unwrapping passed: both directions, accumulator wrap, pause and reset");
    $finish;
  end
endmodule
