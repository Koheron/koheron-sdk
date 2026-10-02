`timescale 1ns/1ps
module test_phase_round;
  reg clk=0;
  always #2.5 clk=~clk;
  reg reset_n=0;
  reg signed [39:0] phase=0;
  reg [7:0] random_round=0;
  wire signed [31:0] rounded;
  phase_round dut(clk,reset_n,phase,random_round,rounded);
  longint signed value, expected, sum;
  integer i,r;
  initial begin
    repeat(4) @(negedge clk);
    reset_n=1;
    // Exhaust every random outcome for each fraction, both signs and both
    // signed modulo boundaries. Compare in wide arithmetic before casting.
    for(i=0;i<1032;i=i+1) begin
      if(i<1024) value=i-512;
      else case(i)
        1024: value=40'sh7fffffffff;
        1025: value=-40'sh8000000000;
        1026: value=40'sh7fffffff00;
        1027: value=40'sh7fffffff80;
        1028: value=-40'sh8000000000+1;
        1029: value=-40'sh8000000000+128;
        1030: value=40'sh4000000001;
        1031: value=-40'sh4000000001;
      endcase
      sum=0;
      for(r=0;r<256;r=r+1) begin
        @(negedge clk);phase=value;random_round=r;
        expected=(value+r)>>>8;
        @(posedge clk);#1;
        if(rounded!==expected[31:0]) $fatal(1,"Signed stochastic phase rounding failed: %d %d",value,r);
        if(i<1024) sum=sum+longint'(rounded);
      end
      // Mean across uniform random words is exactly the fractional input.
      if(i<1024 && sum!==value) $fatal(1,"Phase rounding is biased for %d: sum %d",value,sum);
    end
    @(negedge clk);reset_n=0;
    @(posedge clk);#1;
    if(rounded!==0) $fatal(1,"Phase rounding reset failed");
    $display("Phase rounding checks passed: exhaustive fractions, signs, mean and modulo boundaries");
    $finish;
  end
endmodule
