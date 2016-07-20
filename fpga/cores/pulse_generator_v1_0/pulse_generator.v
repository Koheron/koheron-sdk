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
  output reg dout
);

reg [PULSE_WIDTH_WIDTH-1:0] pulse_width_reg;
reg [PULSE_PERIOD_WIDTH-1:0] pulse_period_reg;
reg [PULSE_PERIOD_WIDTH-1:0] cnt;
initial cnt = 0;

always @(posedge clk) begin
  pulse_width_reg <= pulse_width;
  pulse_period_reg <= pulse_period;
end


always @(posedge clk) begin
  dout <= (cnt < pulse_width_reg);
  if (rst) begin
    cnt <= 0;
  end else begin
    if (cnt < pulse_period_reg - 1) begin
      cnt <= cnt + 1;
    end else begin
      cnt <= 0;      
    end
  end
end
  
endmodule
