`timescale 1ns/1ps
module test_phase_history_tb;
    reg clk=0,rst=1,acc_on=0;
    always #2 clk=~clk;
    reg [64:0] add_x=0,add_y=0;
    reg add_cin=0;
    wire [64:0] add_result;
    phase_unwrapper_adder #(.WIDTH(65),.BLOCK(8)) carry_check(
        add_x,add_y,add_cin,add_result);
    reg signed [23:0] phase_in=0;
    wire signed [24:0] frequency[0:2];
    wire signed [63:0] history[0:2];
    wire overflow[0:2];
    phase_unwrapper #(.DIN_WIDTH(24),.DOUT_WIDTH(64)) ripple(
        clk,acc_on,rst,phase_in,frequency[0],history[0],overflow[0]);
    phase_unwrapper #(.DIN_WIDTH(24),.DOUT_WIDTH(64),.PIPELINED_OVERFLOW(1),.LOOKAHEAD_HISTORY(1)) bounded(
        clk,acc_on,rst,phase_in,frequency[1],history[1],overflow[1]);
    phase_unwrapper #(.DIN_WIDTH(24),.DOUT_WIDTH(64),.PIPELINED_OVERFLOW(1),.PIPELINED_HISTORY(1)) piped(
        clk,acc_on,rst,phase_in,frequency[2],history[2],overflow[2]);
    // Exercise the combined extractor's fused difference with the wide,
    // pipelined monitor history. Delay the fused input by one clock so its
    // frequency, history and overflow must match the conventional path.
    reg signed [23:0] delayed_phase=0, delayed_canonical=0;
    wire signed [23:0] canonical={{2{phase_in[21]}},phase_in[21:0]};
    always @(posedge clk) begin
        delayed_phase<=phase_in;
        delayed_canonical<=canonical;
    end
    wire signed [24:0] fused_frequency[0:2];
    wire signed [63:0] fused_history[0:2];
    wire fused_overflow[0:2];
    phase_unwrapper #(.DIN_WIDTH(24),.DOUT_WIDTH(64),.PIPELINED_OVERFLOW(1),
        .PIPELINED_HISTORY(1),.FUSED_DIFFERENCE(1)) general_fused(
        clk,acc_on,rst,delayed_phase,fused_frequency[0],fused_history[0],fused_overflow[0]);
    phase_unwrapper #(.DIN_WIDTH(24),.DOUT_WIDTH(64),.PIPELINED_OVERFLOW(1),
        .PIPELINED_HISTORY(1)) canonical_reference(
        clk,acc_on,rst,canonical,fused_frequency[1],fused_history[1],fused_overflow[1]);
    phase_unwrapper #(.DIN_WIDTH(24),.DOUT_WIDTH(64),.PIPELINED_OVERFLOW(1),
        .PIPELINED_HISTORY(1),.FUSED_DIFFERENCE(1),.CANONICAL_INPUT(1)) canonical_fused(
        clk,acc_on,rst,delayed_canonical,fused_frequency[2],fused_history[2],fused_overflow[2]);
    reg signed [23:0] previous=0;
    reg signed [24:0] difference=0,increment=0;
    reg signed [64:0] next_history;
    reg signed [63:0] expected=0,expected_d=0;
    reg sticky=0,sticky_d=0,sticky_dd=0;
    integer cycle=0,seed=31982,carry_seed=19481;
    always @(posedge clk) begin
        expected_d=expected;sticky_dd=sticky_d;
        next_history=$signed(expected)+$signed(increment);
        if(rst) begin expected=0;expected_d=0;sticky=0;sticky_d=0;sticky_dd=0;end
        else begin
            sticky_d=sticky;
            if(acc_on) begin
                expected=next_history[63:0];
                sticky=sticky | (next_history[64]!=next_history[63]);
            end
        end
        if(difference>25'sd2097152) increment=difference-25'sd4194304;
        else if(difference< -25'sd2097152) increment=difference+25'sd4194304;
        else increment=difference;
        difference=$signed(phase_in)-$signed(previous);previous=phase_in;
        #1;
        if(history[0]!==expected || history[1]!==expected ||
           frequency[0]!==increment || frequency[1]!==increment ||
           overflow[0]!==sticky || overflow[1]!==sticky_d ||
           history[2]!==expected_d || frequency[2]!==increment || overflow[2]!==sticky_dd)
            $fatal(1,"Phase history arithmetic/reset/overflow mismatch cycle=%0d",cycle);
        if(fused_frequency[0]!==frequency[2] || fused_history[0]!==history[2] ||
           fused_overflow[0]!==overflow[2] || fused_frequency[1]!==fused_frequency[2] ||
           fused_history[1]!==fused_history[2] || fused_overflow[1]!==fused_overflow[2])
            $fatal(1,"Fused wide phase history mismatch cycle=%0d",cycle);
        cycle=cycle+1;
    end
    initial begin
        // Arbitrary full-width words exercise every local carry and selection,
        // including high history bits that a short random phase walk cannot reach.
        for(integer k=0;k<5000;k=k+1) begin
            add_x={$random(carry_seed),$random(carry_seed),$random(carry_seed)};
            add_y={$random(carry_seed),$random(carry_seed),$random(carry_seed)};
            add_cin=$random(carry_seed);
            #1;
            if(add_result!==(add_x+add_y+add_cin)) $fatal(1,"Full-width carry mismatch");
        end
        repeat(5) @(negedge clk);rst=0;acc_on=1;
        // Seed an otherwise unreachable history boundary, then exercise the
        // normal frequency pipeline and accumulator across signed overflow.
        ripple.phase_state=64'h7fffffffffffffff;
        bounded.lookahead_history.history_add.phase=64'h7fffffffffffffff;
        bounded.lookahead_history.history_add.zero_prefix=1;
        bounded.lookahead_history.history_add.one_prefix='1;
        piped.phase_state=64'h7fffffffffffffff;
        piped.split_history.low_state=32'hffffffff;
        general_fused.phase_state=64'h7fffffffffffffff;
        general_fused.split_history.low_state=32'hffffffff;
        expected=64'h7fffffffffffffff;
        for(integer k=0;k<16;k=k+1) begin
            case(k)
                0,2,4,6,8,10,12,14: phase_in=0;
                1: phase_in=24'sd2097152;
                3: phase_in=-24'sd2097152;
                5: phase_in=24'sd2097153;
                7: phase_in=-24'sd2097153;
                9: phase_in=24'sd2097151;
                11: phase_in=-24'sd2097151;
                13: phase_in=24'sh7fffff;
                15: phase_in=24'sh800000;
            endcase
            @(negedge clk);
        end
        for(integer k=0;k<10000;k=k+1) begin
            phase_in=($random(seed) & 24'h3fffff)-24'sd2097152;
            acc_on=(k%11!=0);rst=(k%997==996);
            @(negedge clk);
        end
        $display("Phase history checks passed: 5000 full-width sums, pi boundaries, full-width differences, 10000 random samples, 64-bit overflow, enables, reset and fused/canonical wide history");
        $finish;
    end
endmodule
