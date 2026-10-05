`timescale 1 ns / 1 ps

// H(z) = ((1 + z^-1 + ... + z^-15)/16)^4.
// No interstage truncation: four sums add 16 bits of precision.
// One sample per clock, five pipeline clocks, unity DC gain.
module phase_prefilter #(
  parameter integer WIDTH = 16
) (
  input wire clk,
  input wire aresetn,
  input wire signed [WIDTH-1:0] din,
  input wire [15:0] random_round,
  output reg signed [WIDTH-1:0] dout
);
  wire signed [WIDTH-1:0] stage0 = din;
  wire signed [WIDTH+3:0] stage1;
  wire signed [WIDTH+7:0] stage2;
  wire signed [WIDTH+11:0] stage3;
  wire signed [WIDTH+15:0] stage4;

  phase_moving_sum #(.WIDTH(WIDTH)) s0(clk, aresetn, stage0, stage1);
  phase_moving_sum #(.WIDTH(WIDTH+4)) s1(clk, aresetn, stage1, stage2);
  phase_moving_sum #(.WIDTH(WIDTH+8)) s2(clk, aresetn, stage2, stage3);
  phase_moving_sum #(.WIDTH(WIDTH+12)) s3(clk, aresetn, stage3, stage4);

  // Uniform [0, 65535] rounding makes either sign unbiased. Extend before
  // adding, so a positive full-scale input cannot overflow the signed sum.
  wire signed [WIDTH+16:0] rounded = $signed({stage4[WIDTH+15], stage4})
                                 + $signed({{(WIDTH+1){1'b0}}, random_round});
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
