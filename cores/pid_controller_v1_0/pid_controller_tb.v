`timescale 1 ns / 1 ps

module pid_controller_tb();
  
  parameter DATA_WIDTH = 14;

  reg                   clk;
  reg                   rst;
  reg [DATA_WIDTH-1:0]  data_in;
  reg [DATA_WIDTH-1:0]  set_point;
  reg [DATA_WIDTH-1:0]  p_coef;
  reg [DATA_WIDTH-1:0]  i_coef;
  reg [DATA_WIDTH-1:0]  d_coef;

  wire [2*DATA_WIDTH+1-1:0] data_out;

  pid_controller #(
    .DATA_WIDTH(DATA_WIDTH)
  ) DUT (
    .clk(clk),
    .rst(rst),
    .data_in(data_in),
    .set_point(set_point),
    .p_coef(p_coef),
    .i_coef(i_coef),
    .d_coef(d_coef),
    .data_out(data_out)    
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    rst = 0;
    data_in = $signed(2048);
    set_point = $signed(4096);
    p_coef = $signed(4096);
    i_coef = $signed(1);
    d_coef = $signed(1);

    #(10*CLK_PERIOD) p_coef=$signed(4096);
    #(10*CLK_PERIOD) p_coef=$signed(128);
    #(100*CLK_PERIOD)
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


