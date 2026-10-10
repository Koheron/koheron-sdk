`timescale 1ns/1ps
module lookahead_case #(parameter WIDTH=64, STEP_WIDTH=17)(output reg done=0);
    localparam GROUPS=(WIDTH-STEP_WIDTH+3)/4;
    reg clk=0, rst=1, enable=0;
    always #2 clk=~clk;
    wire signed [WIDTH-1:0] state;
    reg signed [WIDTH-1:0] expected=0;
    reg signed [STEP_WIDTH-1:0] step=0;
    reg [WIDTH-1:0] expected_sum;
    integer seed=31891+WIDTH+STEP_WIDTH, checked=0;
    reg z,o;
    phase_unwrapper_lookahead #(.WIDTH(WIDTH),.STEP_WIDTH(STEP_WIDTH)) dut(
        .clk(clk),.rst(rst),.enable(enable),.step(step),.phase(state));
    always @(posedge clk) begin
        expected_sum=$signed(expected)+$signed(step);
        if (dut.sum!==expected_sum) $fatal(1,"Incorrect sum WIDTH=%0d STEP=%0d cycle=%0d",WIDTH,STEP_WIDTH,checked);
        if (rst) expected=0;
        else if (enable) expected=expected_sum;
        #1;
        if (state!==expected) $fatal(1,"Incorrect registered state");
        // Check every cached predicate after every edge, including holds and
        // reset, so a latent prediction error cannot escape the sum checks.
        for (integer g=0;g<GROUPS;g=g+1) begin
            z=1; o=1;
            for (integer b=STEP_WIDTH;b<STEP_WIDTH+4*g;b=b+1) begin
                z=z && !expected[b]; o=o && expected[b];
            end
            if (dut.zero_prefix[g]!==z || dut.one_prefix[g]!==o)
                $fatal(1,"Incorrect prefix WIDTH=%0d STEP=%0d group=%0d cycle=%0d",WIDTH,STEP_WIDTH,g,checked);
        end
        checked=checked+1;
    end
    task seed_state(input [WIDTH-1:0] value);
        reg zero_flag,one_flag;
        begin
            @(negedge clk);
            rst=0; enable=0; dut.phase=value; expected=value;
            // Long-history boundaries are unreachable in a short simulation.
            // Initialize the complete accumulator state, including its cache.
            for (integer g=0;g<GROUPS;g=g+1) begin
                zero_flag=1; one_flag=1;
                for (integer b=STEP_WIDTH;b<STEP_WIDTH+4*g;b=b+1) begin
                    zero_flag=zero_flag && !value[b];
                    one_flag=one_flag && value[b];
                end
                dut.zero_prefix[g]=zero_flag;
                dut.one_prefix[g]=one_flag;
            end
        end
    endtask
    initial begin
        repeat (4) @(negedge clk);
        if (WIDTH<=8) begin
            for (integer a=0;a<(1<<WIDTH);a=a+1)
                for (integer b=0;b<(1<<STEP_WIDTH);b=b+1) begin
                    seed_state(a); step=b; enable=1;
                    @(negedge clk); enable=0;
                end
        end
        for (integer b=0;b<WIDTH;b=b+1) begin
            seed_state(({{(WIDTH-1){1'b0}},1'b1}<<b)-1); step=1; enable=1;
            repeat (4) @(negedge clk);
            step=-1;
            repeat (8) @(negedge clk);
            step=1;
            repeat (8) @(negedge clk);
        end
        for (integer epoch=0;epoch<12;epoch=epoch+1) begin
            seed_state({$random(seed),$random(seed),$random(seed)});
            for (integer k=0;k<2000;k=k+1) begin
                case (k%5)
                    0: step={1'b1,{(STEP_WIDTH-1){1'b0}}};
                    1: step={1'b0,{(STEP_WIDTH-1){1'b1}}};
                    default: step=$random(seed);
                endcase
                enable=(k%11!=0);
                rst=(k%997==996);
                @(negedge clk);
            end
        end
        $display("PASS lookahead WIDTH=%0d STEP_WIDTH=%0d cycles=%0d",WIDTH,STEP_WIDTH,checked);
        done=1;
    end
endmodule

module test_lookahead;
    wire [5:0] done;
    lookahead_case #(8,3) exhaustive(done[0]);
    lookahead_case #(64,17) pna(done[1]);
    lookahead_case #(64,25) full_history(done[2]);
    lookahead_case #(32,2) upper_history(done[3]);
    lookahead_case #(65,17) partial_group(done[4]);
    lookahead_case #(4,3) single_group(done[5]);
    initial begin
        wait (&done);
        $display("PASS registered lookahead arithmetic and prefix prediction");
        $finish;
    end
    initial begin
        #1000000;
        $fatal(1,"Lookahead test timed out");
    end
endmodule
