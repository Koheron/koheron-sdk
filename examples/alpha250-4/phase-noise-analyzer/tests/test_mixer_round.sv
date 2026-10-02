`timescale 1ns/1ps
module test_mixer_round;
  reg clk=0;
  always #2.5 clk=~clk;
  reg reset_n=0;
  reg signed [32:0] product=0;
  reg [16:0] random_round=0;
  wire signed [15:0] rounded;
  phase_round #(.INPUT_WIDTH(33),.SHIFT(17),.OUTPUT_WIDTH(16))
    dut(clk,reset_n,product,random_round,rounded);
  longint signed value, expected, sum, lower;
  integer i,r;
  initial begin
    repeat(4) @(negedge clk);
    reset_n=1;
    // Actual 16x16 complex products fit +/-2^31. ADC input is real,
    // so its physical range is narrower still. Exhaust all 17-bit dither
    // values at sign, half-LSB and product extremes; no statistical tolerance.
    for(i=0;i<24;i=i+1) begin
      case(i)
        0: value=0;
        1: value=1;
        2: value=-1;
        3: value=65535;
        4: value=65536;
        5: value=65537;
        6: value=-65535;
        7: value=-65536;
        8: value=-65537;
        9: value=131071;
        10: value=131072;
        11: value=131073;
        12: value=-131071;
        13: value=-131072;
        14: value=-131073;
        15: value=1073741824;
        16: value=1073741823;
        17: value=-1073741824;
        18: value=-1073741823;
        19: value=2147483648;
        20: value=2147483647;
        21: value=-2147483648;
        22: value=-2147483647;
        23: value=123456789;
      endcase
      sum=0;lower=value>>>17;
      for(r=0;r<131072;r=r+1) begin
        @(negedge clk);product=value;random_round=r;
        expected=(value+r)>>>17;
        @(posedge clk);#1;
        if(longint'(rounded)!==expected) $fatal(1,"Mixer signed rounding: product %d random %d output %d expected %d",value,r,rounded,expected);
        if(rounded!=lower && rounded!=lower+1) $fatal(1,"Output is not an adjacent count");
        sum=sum+longint'(rounded);
      end
      if(sum!==value) $fatal(1,"Biased mixer mean: product %d sum %d",value,sum);
    end
    @(negedge clk);reset_n=0;
    @(posedge clk);#1;
    if(rounded!==0) $fatal(1,"Mixer rounding reset failed");
    $display("Mixer rounding checks passed: all 131072 dither values, 24 signed products, exact unbiased means and reset");
    $finish;
  end
endmodule
