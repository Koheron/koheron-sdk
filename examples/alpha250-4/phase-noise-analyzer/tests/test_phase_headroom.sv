`timescale 1ns/1ps
module test_phase_headroom;
    reg clk=0; always #2.5 clk=~clk;
    reg rst=1;
    reg signed [15:0] old_input=0;
    reg signed [15:0] input_phase=0;
    wire signed [31:0] old_phase;
    wire signed [63:0] phase;
    wire old_overflow, overflow;
    phase_unwrapper old_unwrap(clk,1'b1,rst,old_input,,old_phase,old_overflow);
    phase_unwrapper #(.DIN_WIDTH(16),.DOUT_WIDTH(64)) wide_unwrap(
        clk,1'b1,rst,input_phase,,phase,overflow);
    reg signed [64:0] difference=0;
    wire signed [31:0] limited;
    wire range_overflow;
    phase_range_guard guard(clk,!rst,difference,limited,range_overflow);
    integer i,wrapped;
    longint expected;
    task check_range(input reg signed [64:0] v,
                     input reg signed [31:0] result,input reg flag);
        begin
            @(negedge clk); difference=v;
            @(posedge clk); #1;
            if(limited!==result || range_overflow!==flag)
                $fatal(1,"Differential range/saturation failure: %h %h %d",v,limited,range_overflow);
        end
    endtask
    initial begin
        repeat(6) @(negedge clk);
        rst=0;
        // Accelerate elapsed carrier phase to the old signed-32 boundary.
        old_unwrap.phase_state=32'h7fffff00;
        wide_unwrap.phase_state=64'h000000007fffff00;
        for(i=0;i<1000;i=i+1) begin
            @(negedge clk);
            wrapped=((i*13+8192)%16384)-8192;
            old_input=wrapped; input_phase=wrapped;
            @(posedge clk); #1;
            if(i>=3) begin
                expected=64'h000000007fffff00+longint'(i-2)*13;
                if(phase!==expected || overflow) $fatal(1,"Wide accumulator lost phase at old boundary");
            end
        end
        if(!old_overflow) $fatal(1,"Old accumulator overflow was not reproduced");
        @(negedge clk);rst=1;old_input=0;input_phase=0;
        repeat(6) @(negedge clk);
        rst=0;
        old_unwrap.phase_state=32'h80000100;
        wide_unwrap.phase_state=-64'sh000000007fffff00;
        for(i=0;i<1000;i=i+1) begin
            @(negedge clk);
            wrapped=((-i*13+8192)%16384+16384)%16384-8192;
            old_input=wrapped;input_phase=wrapped;
            @(posedge clk);#1;
            if(i>=3) begin
                expected=-64'sh000000007fffff00-longint'(i-2)*13;
                if(phase!==expected || overflow) $fatal(1,"Wide accumulator lost negative phase");
            end
        end
        if(!old_overflow) $fatal(1,"Old negative overflow was not reproduced");
        // Large identical carrier phases cancel before range narrowing.
        difference=$signed(65'h1000000000000000)+1234567-$signed(65'h1000000000000000);
        @(posedge clk); #1;
        if(limited!==32'd1234567 || range_overflow) $fatal(1,"Common carrier did not cancel");
        check_range(65'sh000000007fffffff,32'h7fffffff,0);
        check_range(65'sh0000000080000000,32'h7fffffff,1);
        check_range(-65'sh0000000080000000,32'h80000000,0);
        check_range(-65'sh0000000080000001,32'h80000000,1);
        check_range(65'shfffffffffffffff,32'h7fffffff,1);
        check_range(-65'shfffffffffffffff,32'h80000000,1);
        @(negedge clk);rst=1;
        @(posedge clk);#1;
        if(phase!==0 || overflow || limited!==0 || range_overflow)
            $fatal(1,"Wide phase reset failed");
        $display("Phase headroom checks passed: reproduced old overflow; wide phase, common-carrier cancellation and signed range guards");
        $finish;
    end
endmodule
