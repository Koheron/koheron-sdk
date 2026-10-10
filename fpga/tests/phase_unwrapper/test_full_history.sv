`timescale 1ns/1ps
module full_history_case #(parameter DIN_WIDTH=16)(output reg done=0);
    localparam STEP_WIDTH=DIN_WIDTH+1;
    localparam GROUPS=(64-STEP_WIDTH+3)/4;
    reg clk=0, rst=1, enable=1;
    always #2 clk=~clk;
    reg signed [DIN_WIDTH-1:0] phase_in=0;
    wire signed [63:0] phase0,phase1,phase2;
    wire signed [DIN_WIDTH:0] freq0,freq1,freq2;
    wire overflow0,overflow1,overflow2;
    phase_unwrapper #(.DIN_WIDTH(DIN_WIDTH),.DOUT_WIDTH(64)) reference(
        clk,enable,rst,phase_in,freq0,phase0,overflow0);
    phase_unwrapper #(.DIN_WIDTH(DIN_WIDTH),.DOUT_WIDTH(64),.PIPELINED_OVERFLOW(1),.LOOKAHEAD_HISTORY(1)) dut(
        clk,enable,rst,phase_in,freq1,phase1,overflow1);
    phase_unwrapper #(.DIN_WIDTH(DIN_WIDTH),.DOUT_WIDTH(64),.PIPELINED_OVERFLOW(1)) legacy(
        clk,enable,rst,phase_in,freq2,phase2,overflow2);
    reg previous_overflow=0;
    integer detected=0, seed=12819+DIN_WIDTH;
    always @(posedge clk) begin
        #1;
        if (phase0!==phase1 || freq0!==freq1) $fatal(1,"Full history sample/latency mismatch");
        if (phase2!==phase1 || freq2!==freq1 || overflow2!==overflow1)
            $fatal(1,"Lookahead enabled/disabled mismatch");
        if (overflow1!==(rst ? 1'b0 : previous_overflow)) $fatal(1,"Full history overflow mismatch");
        if (overflow0 && !previous_overflow) detected=detected+1;
        previous_overflow=overflow0;
    end
    task seed_history(input [63:0] value);
        reg z,o;
        begin
            reference.phase_state=value; legacy.phase_state=value;
            dut.lookahead_history.history_add.phase=value;
            for (integer g=0;g<GROUPS;g=g+1) begin
                z=1; o=1;
                for (integer b=STEP_WIDTH;b<STEP_WIDTH+4*g;b=b+1) begin
                    z=z && !value[b]; o=o && value[b];
                end
                dut.lookahead_history.history_add.zero_prefix[g]=z;
                dut.lookahead_history.history_add.one_prefix[g]=o;
            end
        end
    endtask
    initial begin
        repeat (8) @(negedge clk);
        for (integer direction=0;direction<2;direction=direction+1) begin
            rst=1; phase_in=0; enable=1;
            repeat (8) @(negedge clk);
            rst=0;
            seed_history(direction ? 64'h8000000000000100 : 64'h7fffffffffffff00);
            for (integer k=0;k<400;k=k+1) begin
                phase_in=direction ? -k*13 : k*13;
                enable=(k%7!=3);
                @(negedge clk);
            end
        end
        rst=1; repeat (8) @(negedge clk); rst=0;
        for (integer k=0;k<20000;k=k+1) begin
            phase_in=$random(seed);
            enable=(k%13<7);
            rst=(k%997==996);
            @(negedge clk);
        end
        if (detected!=2) $fatal(1,"Missing signed overflow coverage");
        $display("PASS full 64-bit history DIN_WIDTH=%0d: identical samples, both overflow boundaries, holds and reset",DIN_WIDTH);
        done=1;
    end
endmodule

module test_full_history;
    wire [1:0] done;
    full_history_case #(16) pna(done[0]);
    full_history_case #(24) wide_phase(done[1]);
    initial begin
        wait (&done);
        $display("PASS full history equivalence");
        $finish;
    end
    initial begin
        #1000000;
        $fatal(1,"Full history test timed out");
    end
endmodule
