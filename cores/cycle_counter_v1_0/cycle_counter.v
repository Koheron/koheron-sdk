`timescale 1 ns / 1 ps

module cycle_counter #
(
  parameter integer FAST_COUNT_WIDTH = 13,
  parameter integer SLOW_COUNT_WIDTH = 19
)
(
  input  wire                         sclr,
  input  wire                         clken,
  input  wire [FAST_COUNT_WIDTH-1:0]  count_max,
  input  wire                         clk,
  output reg                          end_cycle,
  output reg  [FAST_COUNT_WIDTH-1:0]  fast_count,
  output reg  [SLOW_COUNT_WIDTH-1:0]  slow_count
);

  reg [FAST_COUNT_WIDTH-1:0] count_max_reg;

  always @(posedge clk) begin
    if (sclr) begin
      fast_count <= 0;
      slow_count <= 0;
      end_cycle  <= 0;
      count_max_reg <= count_max;
    end else begin
      if (clken) begin
        if (fast_count != count_max) begin
          fast_count <= fast_count + 1;
          if (fast_count == (count_max - 1)) begin
            end_cycle <= 1;
          end else begin 
            end_cycle <= 0;
          end
        end else begin
          fast_count <= 0;
          slow_count <= slow_count + 1;
          end_cycle <= 0;
        end
      end
    end
  end

endmodule


