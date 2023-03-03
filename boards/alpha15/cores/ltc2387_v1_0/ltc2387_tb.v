`timescale 1 ns / 1 ps

module ltc2387_tb();

  reg clk;
  reg din0_a;
  reg din0_b;
  reg din0_co;
  reg din1_a;
  reg din1_b;
  reg din1_co;
  reg clkout_dec;
  wire clkout;
  wire [17:0] adc0;
  wire [17:0] adc1;
  wire adc_valid;

  parameter CLK_PERIOD = 4;

  ltc2387 #(
  )
  DUT (
    .clk(clk),
    .din0_a(din0_a),
    .din0_b(din0_b),
    .din0_co(din0_co),
    .din1_a(din1_a),
    .din1_b(din1_b),
    .din1_co(din1_co),
    .clkout(clkout),
    .clkout_dec(clkout_dec),
    .adc0(adc0),
    .adc1(adc1),
    .adc_valid(adc_valid)
  );

  initial begin
    clk = 1;
    din0_a = 0;
    din0_b = 0;
    din0_co = 0;
    clkout_dec = 0;

    // Period 1
    #(CLK_PERIOD) din0_a = 1; din0_b = 1; din1_a = 1; din1_b = 1;
    #(CLK_PERIOD) din0_a = 1; din0_b = 1; din1_a = 1; din1_b = 1;              //D17 D16
    #(CLK_PERIOD) din0_co = 1; din0_a = 0; din0_b = 0; din1_a = 1; din1_b = 1; // D15 D14
    #(CLK_PERIOD) din0_co = 0; din0_a = 1; din0_b = 1; din1_a = 0; din1_b = 0; // D13 D12
    #(CLK_PERIOD) din0_co = 1; din0_a = 0; din0_b = 0; din1_a = 1; din1_b = 1; // D11 D10
    #(CLK_PERIOD) din0_co = 0; din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0; // D9 D8
    #(CLK_PERIOD) din0_co = 1; din0_a = 1; din0_b = 1; din1_a = 0; din1_b = 0; // D7 D6
    #(CLK_PERIOD) din0_co = 0; din0_a = 1; din0_b = 1; din1_a = 1; din1_b = 1; // D5 D4
    #(CLK_PERIOD) din0_co = 1; din0_a = 1; din0_b = 1; din1_a = 1; din1_b = 1; // D3 D2
    #(CLK_PERIOD) din0_co = 0; din0_a = 0; din0_b = 0; din1_a = 1; din1_b = 1; // D1 D0
    #(CLK_PERIOD) din0_co = 1; din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    #(CLK_PERIOD) din0_co = 0; din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    #(CLK_PERIOD) din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    #(CLK_PERIOD) din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    #(CLK_PERIOD) din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    #(CLK_PERIOD) din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    
    // Period 2
    #(CLK_PERIOD) din0_a = 1; din0_b = 1; din1_a = 1; din1_b = 1;
    #(CLK_PERIOD) din0_a = 1; din0_b = 1; din1_a = 1; din1_b = 1;              //D17 D16
    #(CLK_PERIOD) din0_co = 1; din0_a = 0; din0_b = 0; din1_a = 1; din1_b = 1; // D15 D14
    #(CLK_PERIOD) din0_co = 0; din0_a = 1; din0_b = 1; din1_a = 0; din1_b = 0; // D13 D12
    #(CLK_PERIOD) din0_co = 1; din0_a = 0; din0_b = 0; din1_a = 1; din1_b = 1; // D11 D10
    #(CLK_PERIOD) din0_co = 0; din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0; // D9 D8
    #(CLK_PERIOD) din0_co = 1; din0_a = 1; din0_b = 1; din1_a = 0; din1_b = 0; // D7 D6
    #(CLK_PERIOD) din0_co = 0; din0_a = 1; din0_b = 1; din1_a = 1; din1_b = 1; // D5 D4
    #(CLK_PERIOD) din0_co = 1; din0_a = 1; din0_b = 1; din1_a = 1; din1_b = 1; // D3 D2
    #(CLK_PERIOD) din0_co = 0; din0_a = 0; din0_b = 0; din1_a = 1; din1_b = 1; // D1 D0
    #(CLK_PERIOD) din0_co = 1; din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    #(CLK_PERIOD) din0_co = 0; din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    #(CLK_PERIOD) din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    #(CLK_PERIOD) din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    #(CLK_PERIOD) din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    #(CLK_PERIOD) din0_a = 0; din0_b = 0; din1_a = 0; din1_b = 0;
    

    $finish;
  end

  always #(CLK_PERIOD/2) clk = ~clk;
endmodule