`timescale 1 ns / 1 ps

module write_enable #
(
  parameter integer BRAM_WIDTH = 13
)
(
  input  wire                  start_acq,
  input  wire [BRAM_WIDTH-1:0] address,
  input  wire                  clk,
  output wire                  wen,
  output reg  [32-1:0]         count_cycle,
  output wire                  start_out
);

  reg [BRAM_WIDTH-1:0] count1;
  reg count1_running;
  reg [BRAM_WIDTH-1:0] count2;
  reg count2_running;
  reg rst;
  reg [31:0] count_cycle_next;

  always @(posedge clk) begin
    if (start_acq) begin
      count1 <= {(BRAM_WIDTH){1'b0}};
      count1_running <= 1'b1;
    end
    else begin 
      if (count1 != {(BRAM_WIDTH){1'b1}}) begin
        count1 <= count1 + 1;
      end else begin
        count1_running <= 1'b0;
      end
    end
  end

  always @(posedge clk) begin
    if (count1_running && (address == {(BRAM_WIDTH){1'b0}})) begin
      rst <= 1'b1;
      count_cycle_next <= 32'b0;
      count_cycle <= count_cycle_next;
    end else if ((address == {(BRAM_WIDTH){1'b0}})) begin
      rst <= 1'b0;
      count_cycle_next <= count_cycle_next + 1;
    end else begin
      rst <= 1'b0;
    end
  end

  always @(posedge clk) begin
    if (rst) begin
      count2 <= {(BRAM_WIDTH){1'b0}};
      count2_running <= 1'b1;
    end else begin 
      if (count2 != {(BRAM_WIDTH){1'b1}}) begin
        count2 <= count2 + 1;
      end else begin
        count2_running <= 1'b0;
      end
    end
  end

  assign wen = count2_running;
  assign start_out = rst;

endmodule
