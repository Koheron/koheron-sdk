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
  output reg                          init,
  output reg                          ready,
  output reg                          wen,
  output reg  [SLOW_COUNT_WIDTH-1:0]  n_avg,
  output reg  [FAST_COUNT_WIDTH+1:0]  address
);

  reg [FAST_COUNT_WIDTH-1:0] count_max_reg;
  reg clken_reg, init_restart, wen_reg, ready_reg;
  reg [FAST_COUNT_WIDTH-1:0] fast_count;
  reg [SLOW_COUNT_WIDTH-1:0] slow_count;
  reg [SLOW_COUNT_WIDTH-1:0] n_avg_reg;

  initial begin
    init_restart <= 0;
    fast_count <= 0;
    slow_count <= 0;
    count_max_reg <= {(FAST_COUNT_WIDTH){1'b1}};
    n_avg_reg <= 0;
    ready_reg <= 1;
    wen_reg <= 0;
  end

  always @(posedge clk) begin
    if (restart) begin
      init_restart <= 1;
      ready_reg <= 0;
    end else if (fast_count == count_max_reg) begin
      if (wen_reg) begin 
        ready_reg <= 1;
      end else begin
        if (init_restart) begin
          init_restart <= 0;
        end
      end    
    end
  end

  always @(posedge clk) begin
    n_avg <= n_avg_reg;
    address <= {fast_count, 2'b0};
    wen <= wen_reg;
    ready <= ready_reg;
  end

  always @(posedge clk) begin
    if ((wen_reg) && (fast_count == (count_max_reg - 2))) begin
      init <= 1;
    end else begin
      init <= 0;
    end
  end

  always @(posedge clk) begin
    clken_reg <= clken;
    if (clken_reg) begin
      if (fast_count == count_max_reg) begin
        fast_count <= 0;
        if (wen_reg) begin
          wen_reg <= 0;
          slow_count <= 0;
          n_avg_reg <= slow_count + 1;
        end else begin
          slow_count <= slow_count + 1;
          if (init_restart) begin
            wen_reg <= 1;
          end
        end
      end else begin
        fast_count <= fast_count + 1;
      end
    end
  end

endmodule


