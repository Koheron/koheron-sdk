`timescale 1ns/1ps
module test_extractor_latency_tb;
    reg clk=0,valid=0;
    always #2 clk=~clk;
    wire mixer_valid,cordic_valid;
    wire [47:0] mixed,polar;
    system_complex_mult_0 mixer(clk,valid,32'h00001000,valid,32'h00007fff,
        1'b1,8'b0,mixer_valid,mixed);
    system_cordic_0 cordic(clk,valid,48'h000000200000,cordic_valid,polar);
    integer cycle=0,sent=-1,mixer_clocks=-1,cordic_clocks=-1;
    always @(posedge clk) begin
        cycle=cycle+1;
        if(valid) sent=cycle;
        #1;
        if(mixer_valid) mixer_clocks=cycle-sent+1;
        if(cordic_valid) cordic_clocks=cycle-sent+1;
    end
    initial begin
        repeat(5) @(negedge clk);valid=1;
        @(negedge clk);valid=0;
        repeat(45) @(negedge clk);
        if(mixer_clocks!=4 || cordic_clocks<=0) $fatal(1,"Missing extractor pulse or mixer latency changed");
        $display("Extractor latency checks passed: mixer=%0d clocks, accurate 24-bit CORDIC=%0d clocks",mixer_clocks,cordic_clocks);
        $finish;
    end
endmodule
