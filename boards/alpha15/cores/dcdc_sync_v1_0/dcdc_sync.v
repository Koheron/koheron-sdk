`timescale 1 ns / 1 ps

module dcdc_sync #
(
  parameter integer DIVIDER = 100,
  parameter integer DIVIDER_HALF = DIVIDER / 2,
  parameter integer WIDTH = $clog2(DIVIDER_HALF + 1)
)
(
  input wire clk,
  input wire en,
  input wire state_out,
  output reg dcdc_clk
);

reg [WIDTH-1:0] count;
initial count = 0;
initial dcdc_clk = 0;

always @(posedge clk) begin
  if (en) begin
    if (count != DIVIDER_HALF - 1) begin
        count <= count + 1;
    end else begin
        count <= 0;
        dcdc_clk <= ~dcdc_clk;
    end
  end else begin
    count <= 0;
    dcdc_clk <= state_out;
  end
end

endmodule