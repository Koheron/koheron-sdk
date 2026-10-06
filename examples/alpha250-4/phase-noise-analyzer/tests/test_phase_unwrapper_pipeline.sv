`timescale 1ns/1ps
module test_phase_unwrapper_pipeline;
    reg clk=0; always #2 clk=~clk;
    reg rst=1, acc_on=1;
    reg signed [15:0] input_phase=0;
    wire signed [31:0] phase0,phase1;
    wire signed [16:0] freq0,freq1;
    wire overflow0,overflow1;
    phase_unwrapper direct(clk,acc_on,rst,input_phase,freq0,phase0,overflow0);
    phase_unwrapper #(.PIPELINED_OVERFLOW(1)) pipelined(clk,acc_on,rst,input_phase,freq1,phase1,overflow1);
    reg previous_overflow=0;
    integer i, direction, detected=0;
    always @(posedge clk) begin
        #0.1;
        if (phase0 !== phase1 || freq0 !== freq1) $fatal(1,"Phase/frequency latency changed");
        if (overflow1 !== (rst ? 1'b0 : previous_overflow)) $fatal(1,"Incorrect delayed sticky overflow");
        if (overflow0 && !previous_overflow) detected=detected+1;
        previous_overflow=overflow0;
    end
    initial begin
        repeat(8) @(negedge clk);
        for (direction=0;direction<2;direction=direction+1) begin
            rst=1; input_phase=0; acc_on=1;
            repeat(8) @(negedge clk);
            rst=0;
            direct.phase_out=direction ? 32'h80000100 : 32'h7fffff00;
            pipelined.phase_out=direct.phase_out;
            for (i=0;i<400;i=i+1) begin
                input_phase=direction ? -i*13 : i*13;
                acc_on=(i%7)!=3;
                @(negedge clk);
            end
        end
        rst=1; repeat(8) @(negedge clk); rst=0;
        for (i=0;i<2000;i=i+1) begin
            input_phase=(i*1777)^i;
            acc_on=(i%13)<7;
            if(i==500 || i==1100) rst=1;
            if(i==520 || i==1120) rst=0;
            @(negedge clk);
        end
        if(detected!=2) $fatal(1,"Overflow boundary coverage missing");
        $display("Unwrapper overflow pipeline checks passed: identical phase/frequency samples, both signed boundaries, stalls and reset");
        $finish;
    end
endmodule
