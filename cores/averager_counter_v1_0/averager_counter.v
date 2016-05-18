`timescale 1 ns / 1 ps

module averager_counter #
(
  parameter integer FAST_COUNT_WIDTH = 13,
  parameter integer SLOW_COUNT_WIDTH = 19
)
(
  input  wire                         restart,
  input  wire                         clken,
  input  wire [FAST_COUNT_WIDTH-1:0]  count_max,
  input  wire                         clk,
  input  wire                         avg_on,
  output reg                          ready,
  output reg                          wen,
  output reg [SLOW_COUNT_WIDTH-1:0]   n_avg,
  output reg [SLOW_COUNT_WIDTH-1:0]   slow_count,
  output reg                          clr_fback,
  output reg                          avg_on_out,
  output wire [FAST_COUNT_WIDTH+1:0]  address
);

  reg [FAST_COUNT_WIDTH-1:0] count_max_reg;
  reg clken_reg, clken_reg_;
  reg init_restart;
  reg [FAST_COUNT_WIDTH-1:0] fast_count;
  reg avg_on_out_reg0, avg_on_out_reg1;

  initial begin
    init_restart <= 0;
    fast_count <= 0;
    slow_count <= 0;
    count_max_reg <= {(FAST_COUNT_WIDTH){1'b1}};
    n_avg <= 0;
    ready <= 1;
    wen <= 0;
  end

  always @(posedge clk) begin
    if (restart && clken_reg) begin
      init_restart <= 1;
      ready <= 0;
    end else if (fast_count == count_max_reg) begin
      if (wen) begin 
        ready <= 1;
      end else begin
        if (init_restart) begin
          init_restart <= 0;
        end
      end
    end
  end

  always @(posedge clk) begin
    if ((wen) && (fast_count == (count_max_reg - 2))) begin
      clr_fback <= ~avg_on;
      avg_on_out_reg0 <= avg_on;
      avg_on_out_reg1 <= avg_on_out_reg0;
    end
  end

  always @(posedge clk) begin
    clken_reg_ <= clken;
    clken_reg <= clken_reg_;
    if (clken_reg) begin
      if (fast_count == count_max_reg) begin
        fast_count <= 0;
        if (wen) begin
          count_max_reg <= count_max;
          wen <= 0;
          slow_count <= 0;
          n_avg <= slow_count + 1;
          avg_on_out <= avg_on_out_reg1;
        end else begin
          slow_count <= slow_count + 1;
          if (init_restart) begin
            wen <= 1;
          end
        end
      end else begin
        fast_count <= fast_count + 1;
      end
    end
  end

  assign address = {fast_count, 2'b0};

endmodule


