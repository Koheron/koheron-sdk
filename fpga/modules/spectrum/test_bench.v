`timescale 1 ns / 1 ps

module averager_tb();

  parameter FFT_WIDTH = 12;
  parameter ADC_WIDTH = 14;
  
  reg clk;
  reg [ADC_WIDTH-1:0] adc1;
  reg [ADC_WIDTH-1:0] adc2;
  reg [32-1:0] cfg_sub;
  reg [32-1:0] cfg_fft;
  reg [32-1:0] demod_data;
  reg tvalid;

  wire [32-1:0] m_axis_result_tdata;
  wire m_axis_result_tvalid;

  system_wrapper
  DUT (
    .clk(clk),
    .adc1(adc1),
    .adc2(adc2),
    .cfg_sub(cfg_sub),
    .cfg_fft(cfg_fft),
    .demod_data(demod_data),
    .tvalid(tvalid),
    .m_axis_result_tdata(m_axis_result_tdata),
    .m_axis_result_tvalid(m_axis_result_tvalid)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 0;
    adc2 = 0;
    cfg_sub = 0;
    cfg_fft = 0;
    demod_data = 128;
    tvalid = 0;    
    #(CLK_PERIOD * 2**(FFT_WIDTH-1)) tvalid = 1;
    #(CLK_PERIOD * 2**(FFT_WIDTH-1)) tvalid = 0;
    #(CLK_PERIOD * 2**(FFT_WIDTH-1)) tvalid = 1;
    #(100000*CLK_PERIOD)
    
    $finish;
  end
  
  always #(CLK_PERIOD/2) clk = ~clk;
  
  always begin
    #(CLK_PERIOD * 2) adc1 = 128;
    #(CLK_PERIOD * 2) adc1 = 0;
  end   

endmodule


