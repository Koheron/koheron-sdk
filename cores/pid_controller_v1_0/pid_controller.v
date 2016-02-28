`timescale 1 ns / 1 ps

module pid_controller #
(
  parameter DATA_WIDTH = 14
)
(
  input wire clk,
  input wire rst,
  input wire signed [DATA_WIDTH-1:0] data_in,
  input wire signed [DATA_WIDTH-1:0] set_point,
  input wire signed [DATA_WIDTH-1:0] p_coef,
  input wire signed [DATA_WIDTH-1:0] i_coef,
  input wire signed [DATA_WIDTH-1:0] d_coef,

  output reg  [2*DATA_WIDTH+1-1:0] data_out
);

  localparam integer ERROR_WIDTH = DATA_WIDTH + 1;
  localparam integer MULT_WIDTH = DATA_WIDTH + ERROR_WIDTH;

  reg [ERROR_WIDTH+1-1:0] error;   // error signal

  reg [MULT_WIDTH-1:0] p_reg;      // proportional signal

  // Error
  always @(posedge clk) begin
    if (rst == 1'b1) begin
      error <= 0;
    end else begin
      error <= set_point - data_in;
      data_out <= p_reg;
    end
  end

  wire [MULT_WIDTH-1:0] p_mult;

  assign p_mult = error * p_coef;

  // Proportional
  always @(posedge clk) begin
    if (rst == 1'b1) begin
      p_reg <= {MULT_WIDTH{1'b0}};
    end else begin
      p_reg <= p_mult;
    end
  end

endmodule
