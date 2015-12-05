`timescale 1 ns / 1 ps

module write_enable #
(
  parameter integer BRAM_WIDTH = 13
)
(
  input  wire                  restart,
  input  wire [BRAM_WIDTH-1:0] address,
  input  wire                  clk,
  output wire                  wen,
  output wire [BRAM_WIDTH-1:0] count,
  output wire                  init
);

  reg [BRAM_WIDTH-1:0] count1;
  reg count1_running;
  reg [BRAM_WIDTH-1:0] count2;
  reg count2_running;
  reg rst;
  reg init_reg;

  always @(posedge clk) begin
    if (restart) begin
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
    if (count1_running && (address == {(BRAM_WIDTH){1'b1}})) begin
      rst <= 1'b1;
    end else begin
      rst <= 1'b0;
    end
  end

  always @(posedge clk) begin
    if (rst) begin
      count2 <= {(BRAM_WIDTH){1'b0}};
      count2_running <= 1'b1;
      init_reg <= 1'b0;
    end else begin 
      if (count2 != {(BRAM_WIDTH){1'b1}}) begin
        count2 <= count2 + 1;
        init_reg <= 1'b0;
        if (count2 == {{(BRAM_WIDTH-2){1'b1}},1'b0,1'b1}) begin
          init_reg <= 1'b1;
        end
      end else begin
        count2_running <= 1'b0;
        init_reg <= 1'b0;
      end
    end
  end

  assign wen = count2_running;
  assign count = count2;
  assign init = init_reg;

endmodule


