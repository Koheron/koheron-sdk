`timescale 1 ns / 1 ps

module psd_counter_tb();

  parameter integer PERIOD = 64;
  parameter integer PERIOD_WIDTH = 6;
  parameter integer N_CYCLES = 8;
  parameter integer N_CYCLES_WIDTH = 3;

  reg clk;
  reg s_axis_tvalid;
  reg [32-1:0] s_axis_tdata;
  wire m_axis_tvalid;
  wire [32-1:0] m_axis_tdata;
  wire [PERIOD_WIDTH+1:0] addr;
  wire [N_CYCLES_WIDTH-1:0] cycle_index;
  wire first_cycle;
  wire last_cycle;

  psd_counter #(
    .PERIOD(PERIOD),
    .PERIOD_WIDTH(PERIOD_WIDTH),
    .N_CYCLES(N_CYCLES),
    .N_CYCLES_WIDTH(N_CYCLES_WIDTH)
  )
  DUT (
    .clk(clk),
    .s_axis_tvalid(s_axis_tvalid),
    .s_axis_tdata(s_axis_tdata),
    .m_axis_tvalid(m_axis_tvalid),
    .m_axis_tdata(m_axis_tdata),
    .addr(addr),
    .first_cycle(first_cycle),
    .last_cycle(last_cycle),
    .cycle_index(cycle_index)
  );

  parameter CLK_PERIOD = 8;

  always #(CLK_PERIOD/2) clk = ~clk;

  initial begin
    clk = 1'b1;
    s_axis_tvalid = 0;
    s_axis_tdata = 0;
    #(166*CLK_PERIOD)
    s_axis_tvalid = 1;
    #(100000*CLK_PERIOD)
    $finish;
  end

endmodule
