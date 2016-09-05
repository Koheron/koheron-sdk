`timescale 1 ns / 1 ps

module pulse_generator #
(
  parameter integer PULSE_WIDTH_WIDTH = 8,
  parameter integer PULSE_PERIOD_WIDTH = 16
)
(
  input wire clk,
  input wire [PULSE_WIDTH_WIDTH-1:0] pulse_width,
  input wire [PULSE_PERIOD_WIDTH-1:0] pulse_period,
  input wire rst,
  output reg valid,
  output reg [PULSE_PERIOD_WIDTH-1:0] cnt,
  output reg start
);

reg [PULSE_PERIOD_WIDTH-1:0] cnt_reg;
initial cnt_reg = 0;

always @(posedge clk) begin
  start <= (cnt_reg == 0);
  cnt <= cnt_reg;
  valid <= (cnt_reg < pulse_width);
end

always @(posedge clk) begin
  if (rst) begin
    cnt_reg <= 0;
  end else begin
    if (cnt_reg < pulse_period - 1) begin
      cnt_reg <= cnt_reg + 1;
    end else begin
      cnt_reg <= 0;
    end
  end
end
  
endmodule
