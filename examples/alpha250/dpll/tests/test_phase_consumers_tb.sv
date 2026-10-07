`timescale 1ns/1ps
module test_phase_consumers_tb;
    reg clk=0,resetn=0,acc_on=1,monitor_resetn=0;
    always #2 clk=~clk;
    reg signed [24:0] frequency=0;
    reg signed [63:0] phase=0;
    wire signed [39:0] feedback;
    wire signed [63:0] monitor_phase,relative_phase;
    wire overflow;
    accurate_phase_consumers consumers(clk,resetn,acc_on,frequency,phase,8'd128,feedback,monitor_phase);
    monitor_phase_origin origin(clk,monitor_resetn,monitor_phase,relative_phase,overflow);
    reg signed [39:0] expected=0;
    integer k;
    always @(posedge clk) begin
        if(!resetn) expected=0;else if(acc_on) expected=expected+frequency;
        #1;
        if(feedback!==expected) $fatal(1,"Monitor reset affected feedback or lost fractional phase");
    end
    initial begin
        repeat(3) @(negedge clk);resetn=1;
        phase=64'sh0010000000000000;
        repeat(4) @(negedge clk);monitor_resetn=1;
        for(k=0;k<1000;k=k+1) begin
            frequency=(k%17)-8;phase=phase+frequency;
            acc_on=(k%31!=0);monitor_resetn=(k%73>4);
            @(negedge clk);
        end
        if(overflow) $fatal(1,"Large shared phase offset overflowed monitor origin");
        $display("Phase consumer checks passed: independent enables, monitor epochs and large phase offsets");
        $finish;
    end
    initial begin #10000;$fatal(1,"Phase consumer timeout");end
endmodule
