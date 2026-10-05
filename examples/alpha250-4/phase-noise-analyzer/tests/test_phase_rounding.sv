`timescale 1ns/1ps
module test_phase_rounding;
    reg clk=0; always #2.5 clk=~clk;
    reg reset_n=0;
    reg signed [23:0] phase=0;
    reg [7:0] random_value=0;
    wire signed [15:0] result;
    phase_stochastic_round rounder(clk,reset_n,phase,random_value,result);
    integer base,frac,r,sum,expected,p;
    initial begin
        repeat(3) @(negedge clk);
        reset_n=1;
        // Exhaust all dropped fractions and all rounding values at negative,
        // zero and positive phases, including both sides of the Pi boundary.
        for(base=-8193;base<=8193;base=base+8193) begin
            for(frac=0;frac<256;frac=frac+1) begin
                sum=0;p=base*256+frac;
                for(r=0;r<256;r=r+1) begin
                    @(negedge clk);phase=p;random_value=r;
                    @(posedge clk);#1;expected=(p+r)>>>8;
                    if(result!==expected) $fatal(1,"Signed phase rounding differs from oracle");
                    sum=sum+$signed(result);
                end
                if(sum!==p) $fatal(1,"Stochastic phase rounding is biased: %d %d",sum,p);
            end
        end
        @(negedge clk);reset_n=0;
        @(posedge clk);#1;
        if(result!==0) $fatal(1,"Phase rounding reset failed");
        $display("Phase rounding checks passed: 196608 signed samples, every fractional code and exact unbiased expectation");
        $finish;
    end
endmodule
