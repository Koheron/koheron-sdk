`timescale 1 ns / 1 ps

module integrator_tb();

  parameter COUNT_WIDTH = 5;

  reg adc_valid;
  reg clk;
  reg integrate;
  reg [18:0] setpoint;
  reg [18:0] adc_data;
  wire [16:0] dac_data;

  integrator #()
  DUT (
    .adc_valid(adc_valid),
    .clk(clk),
    .integrate(integrate),
    .adc_data(adc_data),
    .setpoint(setpoint),
    .dac_data(dac_data)
  );

  parameter CLK_PERIOD = 8;

  initial begin
    clk = 1;
    integrate = 1;
    adc_valid = 0;
    setpoint = 1000;
  
    #(16 * CLK_PERIOD)
    adc_valid = 1;
    adc_data = 1100;
    #(CLK_PERIOD)
    adc_valid = 0;
  
    #(16 * CLK_PERIOD)
    adc_valid = 1;
    adc_data = 1500;
    #(CLK_PERIOD)
    adc_valid = 0;
    
    #(16 * CLK_PERIOD)
    adc_valid = 1;
    adc_data = 2800;
    #(CLK_PERIOD)
    adc_valid = 0;
    
    #(16 * CLK_PERIOD)
    adc_valid = 1;
    adc_data = 10000;
    #(CLK_PERIOD)
    adc_valid = 0;
    
    #(6 * CLK_PERIOD)
    integrate = 0;
    
    #(10 * CLK_PERIOD)
    adc_valid = 1;
    adc_data = 10000;
    #(CLK_PERIOD)
    adc_valid = 0;
    
    #(16 * CLK_PERIOD)
    adc_valid = 1;
    adc_data = 1100;
    #(CLK_PERIOD)
    adc_valid = 0;
    
    #(6 * CLK_PERIOD)
    integrate = 1;
    
    #(10 * CLK_PERIOD)
    adc_valid = 1;
    adc_data = 10000;
    #(CLK_PERIOD)
    adc_valid = 0;
    
    #(16 * CLK_PERIOD)
    adc_valid = 1;
    adc_data = 1500;
    #(CLK_PERIOD)
    adc_valid = 0;
    
    #(16 * CLK_PERIOD)
    adc_valid = 1;
    adc_data = 2800;
    #(CLK_PERIOD)
    adc_valid = 0;
  
    #(100 * CLK_PERIOD)
    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;

endmodule


