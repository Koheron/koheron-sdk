`timescale 1ns/1ps
module chirp_generator_tb;
    reg passed=0;
    reg clk=0;
    always #2 clk=~clk;
    reg aresetn=0, reset=0, trigger=0, seed_write=0;
    reg [63:0] seed_data;
    reg [3:0] seed_index;
    reg [47:0] coefficient=48'h3456789abcde;
    reg [31:0] sample_count=2048;
    wire [47:0] phase;
    wire phase_valid;
    chirp_generator dut(.*);
    reg [63:0] expected_state[0:15];
    reg [111:0] product;
    reg [47:0] expected_phase;
    reg [63:0] value;
    integer i, n, run_index;
    initial begin
        for (run_index=0; run_index<2; run_index=run_index+1) begin
            @(negedge clk); reset=1;
            repeat(4) @(negedge clk);
            aresetn=1; reset=0;
            for (i=0; i<16; i=i+1) begin
                expected_state[i]=64'h00004321deadbeef + i*64'h12345678 + run_index;
                seed_data=expected_state[i]; seed_index=i; seed_write=1;
                @(negedge clk); seed_write=0;
                @(negedge clk);
            end
            trigger=1;
            @(negedge clk); trigger=0;
            n=0; expected_phase=0;
            while (n<sample_count) begin
                @(posedge clk); #1;
                if (phase_valid) begin
                    value=expected_state[n%16];
                    expected_phase=expected_phase + (value>>16) + value[15];
                    product=value*coefficient;
                    expected_state[n%16]=value + (product>>64) + product[63];
                    if (phase !== expected_phase)
                        $fatal(1,"phase mismatch sample %0d: %h != %h",n,phase,expected_phase);
                    n=n+1;
                end
            end
            @(posedge clk); #1;
            if (phase_valid) $fatal(1,"extra chirp sample");
            repeat(32) @(negedge clk);
        end
        $display("PASS chirp_generator: exact recurrence, length, reset and replay");
        passed=1;
        $finish;
    end
    initial begin #100000; $fatal(1,"timeout"); end
endmodule
