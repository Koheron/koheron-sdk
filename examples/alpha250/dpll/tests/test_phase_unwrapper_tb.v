`timescale 1ns/1ps
module unwrap_check #(parameter W=24)(input clk, output reg done=0);
    localparam O=W+2;
    localparam integer PI=1<<(W-3), TWO_PI=1<<(W-2);
    reg signed [W-1:0] input_phase=0;
    reg enable=0, reset=1;
    wire signed [W:0] frequency[0:1];
    wire signed [O-1:0] phase[0:1];
    wire overflow[0:1];
    // A registered extractor output adds one clock before the fused core.
    // Both complete paths must produce identical samples on identical cycles.
    reg signed [W-1:0] delayed_phase=0;
    always @(posedge clk) delayed_phase<=input_phase;
    genvar g;
    generate for(g=0;g<2;g=g+1) begin: implementations
        phase_unwrapper #(.DIN_WIDTH(W),.DOUT_WIDTH(O),.FUSED_DIFFERENCE(g)) dut (
            .clk(clk),.acc_on(enable),.rst(reset),.phase_in(g ? delayed_phase : input_phase),
            .freq_out(frequency[g]),.phase_out(phase[g]),.overflow(overflow[g]));
    end endgenerate
    wire signed [W-1:0] canonical_input={{2{input_phase[W-3]}},input_phase[W-3:0]};
    reg signed [W-1:0] delayed_canonical=0;
    always @(posedge clk) delayed_canonical<=canonical_input;
    wire signed [W:0] canonical_frequency[0:1];
    wire signed [O-1:0] canonical_phase[0:1];
    wire canonical_overflow[0:1];
    generate for(g=0;g<2;g=g+1) begin: canonical_implementations
        phase_unwrapper #(.DIN_WIDTH(W),.DOUT_WIDTH(O),.FUSED_DIFFERENCE(g),.CANONICAL_INPUT(g)) dut (
            .clk(clk),.acc_on(enable),.rst(reset),.phase_in(g ? delayed_canonical : canonical_input),
            .freq_out(canonical_frequency[g]),.phase_out(canonical_phase[g]),.overflow(canonical_overflow[g]));
    end endgenerate
    integer previous=0, difference=0, unwrapped=0, next_difference, sum;
    reg signed [O-1:0] accumulated=0;
    reg sticky=0;
    integer cycle=0, k;
    reg [31:0] random_state=32'h928abcd1+W;
    always @(posedge clk) if(!done) begin
        sum=$signed(accumulated)+unwrapped;
        if(reset) begin accumulated=0; sticky=0; end
        else if(enable) begin
            sticky=sticky | (sum[O]!=sum[O-1]);
            accumulated=sum;
        end
        if(difference>PI) unwrapped=difference-TWO_PI;
        else if(difference< -PI) unwrapped=difference+TWO_PI;
        else unwrapped=difference;
        difference=$signed(input_phase)-previous;
        previous=$signed(input_phase);
        #1;
        for(k=0;k<2;k=k+1)
            if(frequency[k]!==unwrapped || phase[k]!==accumulated || overflow[k]!==sticky)
                $fatal(1,"Unwrapper W=%0d mode=%0d cycle=%0d frequency=%0d/%0d phase=%0d/%0d overflow=%0b/%0b",
                    W,k,cycle,frequency[k],unwrapped,phase[k],accumulated,overflow[k],sticky);
        if(canonical_frequency[0]!==canonical_frequency[1] || canonical_phase[0]!==canonical_phase[1] ||
           canonical_overflow[0]!==canonical_overflow[1])
            $fatal(1,"Canonical unwrap W=%0d cycle=%0d frequency=%0d/%0d phase=%0d/%0d overflow=%0b/%0b",
                W,cycle,canonical_frequency[0],canonical_frequency[1],canonical_phase[0],canonical_phase[1],
                canonical_overflow[0],canonical_overflow[1]);
        cycle=cycle+1;
    end
    integer n, a, b;
    initial begin
        // Exhaust the small-width signed input-pair space, including all
        // borrow/upper-table addresses; larger widths also stress long carries.
        // W=10 separately exhausts its canonical eight-bit angle, including
        // every positive/negative pi tie and sign-extension combination.
        if(W==8 || W==10) begin
            enable=1; reset=0;
            for(a=-128;a<128;a=a+1) begin
                for(b=-128;b<128;b=b+1) begin
                    @(negedge clk); input_phase=a;
                    @(negedge clk); input_phase=b;
                end
            end
        end
        for(n=0;n<60000;n=n+1) begin
            @(negedge clk);
            random_state=random_state^(random_state<<13);
            random_state=random_state^(random_state>>17);
            random_state=random_state^(random_state<<5);
            reset=(n%97==0); enable=(n%11!=0);
            case(n%16)
                0: input_phase=0;
                1: input_phase=PI;
                2: input_phase=0;
                3: input_phase=-PI;
                4: input_phase=0;
                5: input_phase=(1<<(W-1))-1;
                6: input_phase=-(1<<(W-1));
                7: input_phase=(1<<(W/2))-1;
                8: input_phase=1<<(W/2);
                9: input_phase=(1<<(W/2))-1;
                10: input_phase=PI-1;
                11: input_phase=-PI+1;
                default: input_phase=random_state;
            endcase
        end
        @(negedge clk); done=1;
        $display("Unwrapper checks passed: W=%0d, %0d cycles, general/canonical paths, unchanged end-to-end cycles, signed extrema, wraps, pi ties, reset, gating and overflow",W,cycle);
    end
endmodule
module test_phase_unwrapper_tb;
    reg clk=0;
    always #2 clk=~clk;
    wire [4:0] done;
    unwrap_check #(.W(8)) a(clk,done[0]);
    unwrap_check #(.W(16)) b(clk,done[1]);
    unwrap_check #(.W(24)) c(clk,done[2]);
    unwrap_check #(.W(4)) d(clk,done[3]);
    unwrap_check #(.W(10)) e(clk,done[4]);
    initial begin wait(&done); $finish; end
    initial begin #1000000; $fatal(1,"Watchdog"); end
endmodule
