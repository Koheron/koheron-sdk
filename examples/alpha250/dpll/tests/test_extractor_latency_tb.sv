`timescale 1ns/1ps
module test_extractor_latency_tb;
    reg clk=0,valid=0,resetn=0;
    always #2 clk=~clk;
    wire mixer_valid,cordic_valid;
    wire [47:0] mixed;
    wire signed [23:0] phase;
    system_complex_mult_0 mixer(clk,valid,32'h00001000,valid,32'h00007fff,
        1'b1,8'b0,mixer_valid,mixed);
    phase_extractor #(.INPUT_WIDTH(24),.PHASE_WIDTH(24),.ITERATIONS(24),
        .ROTATIONS_PER_CLOCK(2),.PAIR_START(8),.FUSE_ROUND(1),.COMPACT_PREP(0),.RESIDUAL_CORRECTION(1))
        extractor(.clk(clk),.resetn(resetn),.valid_in(valid),.i_in(24'sh200000),.q_in(24'sd0),
                  .valid_out(cordic_valid),.phase_out(phase));
    integer cycle=0,sent=-1,mixer_clocks=-1,cordic_clocks=-1;
    always @(posedge clk) begin
        cycle=cycle+1;
        if(valid) sent=cycle;
        #1;
        if(mixer_valid) mixer_clocks=cycle-sent+1;
        if(cordic_valid) cordic_clocks=cycle-sent+1;
    end
    initial begin
        repeat(5) @(negedge clk);resetn=1;
        repeat(5) @(negedge clk);valid=1;
        @(negedge clk);valid=0;
        repeat(45) @(negedge clk);
        if(mixer_clocks!=4 || cordic_clocks!=15) $fatal(1,"Missing extractor pulse or extraction latency changed");
        $display("Extractor latency checks passed: mixer=%0d clocks, accurate custom extractor=%0d clocks",mixer_clocks,cordic_clocks);
        $finish;
    end
endmodule
