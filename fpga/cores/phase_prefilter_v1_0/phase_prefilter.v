`timescale 1 ns / 1 ps

// H(z) = ((1 + z^-1 + ... + z^-15)/16)^4.
// No interstage truncation: four sums add 16 bits of precision.
// One sample per clock, five pipeline clocks, unity DC gain.
module phase_prefilter (
  input wire clk,
  input wire aresetn,
  input wire signed [15:0] din,
  input wire [15:0] random_round,
  output reg signed [15:0] dout
);
  wire signed [15:0] stage0 = din;
  wire signed [19:0] stage1;
  wire signed [23:0] stage2;
  wire signed [27:0] stage3;
  wire signed [31:0] stage4;

  phase_moving_sum #(.WIDTH(16)) s0(clk, aresetn, stage0, stage1);
  phase_moving_sum #(.WIDTH(20)) s1(clk, aresetn, stage1, stage2);
  phase_moving_sum #(.WIDTH(24)) s2(clk, aresetn, stage2, stage3);
  phase_moving_sum #(.WIDTH(28)) s3(clk, aresetn, stage3, stage4);

  // Uniform [0, 65535] rounding makes either sign unbiased. Extend before
  // adding, so a positive full-scale input cannot overflow the signed sum.
  wire signed [32:0] rounded = $signed({stage4[31], stage4})
                            + $signed({17'b0, random_round});
  always @(posedge clk)
    if (!aresetn) dout <= 0;
    else dout <= rounded >>> 16;
endmodule

module phase_moving_sum #(
  parameter integer WIDTH = 16
) (
  input wire clk,
  input wire aresetn,
  input wire signed [WIDTH-1:0] din,
  output reg signed [WIDTH+3:0] dout
);
  reg signed [WIDTH-1:0] delay [0:15];
  integer i;
  wire signed [WIDTH+3:0] incoming = {{4{din[WIDTH-1]}}, din};
  wire signed [WIDTH+3:0] outgoing = {{4{delay[15][WIDTH-1]}}, delay[15]};
  always @(posedge clk) begin
    if (!aresetn) begin
      dout <= 0;
      for (i = 0; i < 16; i = i + 1) delay[i] <= 0;
    end else begin
      delay[0] <= din;
      for (i = 1; i < 16; i = i + 1) delay[i] <= delay[i-1];
      dout <= dout + incoming - outgoing;
    end
  end
endmodule
