`timescale 1 ns / 1 ps

module address_generator #
(
  parameter integer COUNT_WIDTH = 13
)
(
  input  wire                    sclr,
  input  wire [COUNT_WIDTH-1:0]  count_max,
  input  wire                    clk,
  output wire  [COUNT_WIDTH+1:0] address
);

  reg [COUNT_WIDTH-1:0] count_max_reg;
  reg [COUNT_WIDTH-1:0] count;

  always @(posedge clk) begin
    if (sclr) begin
      count <= 0;
      count_max_reg <= count_max;
    end else begin
      if (count != count_max) begin
        count <= count + 1;
      end else begin
        count <= 0;
        count_max_reg <= count_max;
      end
    end
  end

  assign address = {count, 2'b0};

endmodule
