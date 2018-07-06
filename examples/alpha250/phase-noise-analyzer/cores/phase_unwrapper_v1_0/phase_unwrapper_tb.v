`timescale 1 ns / 1 ps

module phase_unwrapper_tb();

  parameter DIN_WIDTH = 8;
  parameter DOUT_WIDTH = 16;

  reg clk;
  reg acc_on;
  reg signed [DIN_WIDTH-1:0] phase_in;
  wire signed [DIN_WIDTH+1-1:0] freq_out;
  wire signed [DOUT_WIDTH-1:0] phase_out;

  phase_unwrapper #(
    .DIN_WIDTH(DIN_WIDTH),
    .DOUT_WIDTH(DOUT_WIDTH)
  )
  DUT (
    .clk(clk),
    .acc_on(acc_on),
    .phase_in(phase_in),
    .freq_out(freq_out),
    .phase_out(phase_out)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    acc_on = 1;
    phase_in = 0;
    #(CLK_PERIOD) phase_in = 0;
    #(CLK_PERIOD) phase_in = 5;
    #(CLK_PERIOD) phase_in = 10;
    #(CLK_PERIOD) phase_in = 15;
    #(CLK_PERIOD) phase_in = 20;
    #(CLK_PERIOD) phase_in = 25;
    #(CLK_PERIOD) phase_in = 30;
    #(CLK_PERIOD) phase_in = -29;
    #(CLK_PERIOD) phase_in = -24;
    #(CLK_PERIOD) phase_in = -19;
    #(CLK_PERIOD) phase_in = -14;
    #(CLK_PERIOD) phase_in = -9;
    #(CLK_PERIOD) phase_in = -4;
    #(CLK_PERIOD) phase_in = 1;
    #(CLK_PERIOD) phase_in = 6;
    #(CLK_PERIOD) phase_in = 11;
    #(CLK_PERIOD) phase_in = 16;
    #(CLK_PERIOD) phase_in = 21; acc_on = 0;
    #(CLK_PERIOD) phase_in = 26;
    #(CLK_PERIOD) phase_in = 31;
    #(CLK_PERIOD) phase_in = -28;
    #(CLK_PERIOD) phase_in = -23;
    #(CLK_PERIOD) phase_in = -18;
    #(CLK_PERIOD) phase_in = -13;
    #(CLK_PERIOD) phase_in = -8;
    #(CLK_PERIOD) phase_in = -3;
    #(CLK_PERIOD) phase_in = 2;
    #(CLK_PERIOD) phase_in = 7;
    #(CLK_PERIOD) phase_in = 12;
    #(CLK_PERIOD) phase_in = 17;
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


