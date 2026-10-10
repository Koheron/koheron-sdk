`timescale 1ns/1ps
// Independent sample model: no DUT internals or shared arithmetic helpers.
module latency_case #(
    parameter DIN=16, DOUT=64, DELAY_OVERFLOW=1, SPLIT=0,
              FUSED=0, CANONICAL=0, LOOKAHEAD=1
)(output reg done=0);
    reg clk=0, rst=0, enable=0;
    always #2 clk=~clk;
    reg signed [DIN-1:0] input_phase=0, previous_input=0;
    reg signed [DIN:0] difference=0, frequency=0, new_difference;
    reg signed [DOUT-1:0] history=0, expected_phase=0;
    wire signed [DIN:0] actual_frequency;
    wire signed [DOUT-1:0] actual_phase;
    wire overflow;
    integer seed=8719+DIN+DOUT+SPLIT+FUSED, cycles=0;
    phase_unwrapper #(.DIN_WIDTH(DIN),.DOUT_WIDTH(DOUT),
        .PIPELINED_OVERFLOW(DELAY_OVERFLOW),.PIPELINED_HISTORY(SPLIT),
        .FUSED_DIFFERENCE(FUSED),.CANONICAL_INPUT(CANONICAL),
        .LOOKAHEAD_HISTORY(LOOKAHEAD)) dut(
        clk,enable,rst,input_phase,actual_frequency,actual_phase,overflow);

    function automatic signed [DIN:0] unwrap(input signed [DIN:0] value);
        if (value > (1 << (DIN-3))) unwrap=value-(1 << (DIN-2));
        else if (value < -(1 << (DIN-3))) unwrap=value+(1 << (DIN-2));
        else unwrap=value;
    endfunction

    always @(posedge clk) begin
        // A split history exposes the preceding accumulator sample. Full
        // history exposes the sum on this edge, including with lookahead.
        if (SPLIT) expected_phase=rst ? 0 : history;
        if (rst) history=0;
        else if (enable) history=history+$signed(frequency);
        if (!SPLIT) expected_phase=history;
        new_difference=$signed(input_phase)-$signed(previous_input);
        frequency=unwrap(FUSED ? new_difference : difference);
        difference=new_difference;
        previous_input=input_phase;
        #1;
        if (actual_phase!==expected_phase || actual_frequency!==frequency)
            $fatal(1,"Latency/value mismatch DIN=%0d DOUT=%0d split=%0d fused=%0d lookahead=%0d cycle=%0d",
                DIN,DOUT,SPLIT,FUSED,LOOKAHEAD,cycles);
        cycles=cycles+1;
    end
    initial begin
        #1;
        if (actual_phase!==0 || actual_frequency!==0 || overflow!==0)
            $fatal(1,"Outputs must initialize before the first clock/reset");
        @(negedge clk);
        for (integer k=0;k<4096;k=k+1) begin
            // Exercise reset with enable both high and low, short and long
            // holds, and changing inputs while history is stopped/reset.
            enable=(k%23<17);
            rst=(k%127<3);
            input_phase=$random(seed);
            if (CANONICAL)
                input_phase={{2{input_phase[DIN-3]}},input_phase[DIN-3:0]};
            @(negedge clk);
        end
        $display("PASS latency DIN=%0d DOUT=%0d split=%0d fused=%0d canonical=%0d lookahead=%0d",
            DIN,DOUT,SPLIT,FUSED,CANONICAL,LOOKAHEAD);
        done=1;
    end
endmodule

module test_latency;
    wire [11:0] done;
    latency_case #(16,64,1,0,0,0,1) pna(done[0]);
    latency_case #(24,64,1,0,1,0,1) fused_full(done[1]);
    latency_case #(24,64,1,0,1,1,1) canonical_full(done[2]);
    latency_case #(16,64,1,1,0,0,1) split_priority(done[3]);
    latency_case #(24,64,1,1,1,0,0) fused_split(done[4]);
    latency_case #(24,64,1,1,1,1,0) canonical_split(done[5]);
    latency_case #(16,32,1,0,0,0,1) narrow_fallback(done[6]);
    latency_case #(16,64,0,0,0,0,1) overflow_fallback(done[7]);
    latency_case #(16,64,1,0,0,0,0) disabled(done[8]);
    latency_case #(3,3,0,0,0,0,0) minimum_width(done[9]);
    latency_case #(16,65,1,0,0,0,1) partial_group(done[10]);
    latency_case #(32,33,1,0,0,0,1) step_width_fallback(done[11]);
    initial begin
        wait (&done);
        $display("PASS phase/frequency latency and reset/enable contract for all history modes");
        $finish;
    end
    initial begin
        #100000;
        $fatal(1,"Latency test timed out");
    end
endmodule
