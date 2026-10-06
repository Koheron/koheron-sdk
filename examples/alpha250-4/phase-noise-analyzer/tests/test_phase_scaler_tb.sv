`timescale 1ns/1ps
module test_phase_scaler_tb;
    reg clk=0;
    always #2 clk=!clk;
    reg signed [63:0] phase=0;
    reg signed [31:0] scale=0;
    wire [63:0] result;
    system_scaler0_0 scaler(.CLK(clk),.A(phase),.B(scale),.P(result));
    reg signed [95:0] product;
    reg [63:0] expected [0:7];
    integer cycle,stage;
    reg [63:0] random_state=64'h9e3779b97f4a7c15;
    initial begin
        for(stage=0;stage<8;stage=stage+1) expected[stage]=0;
        for(cycle=0;cycle<4096;cycle=cycle+1) begin
            @(negedge clk);
            random_state=random_state^(random_state<<13);
            random_state=random_state^(random_state>>7);
            random_state=random_state^(random_state<<17);
            phase=random_state;
            scale=random_state[63:32];
            case(cycle%16)
                0: begin phase=64'h8000000000000000;scale=32'h80000000;end
                1: begin phase=64'h7fffffffffffffff;scale=32'h7fffffff;end
                2: scale=32'h40000000;
                3: scale=-32'h40000000;
                4: scale=0;
                5: phase=0;
                6: scale=1;
                7: scale=-1;
            endcase
            product=phase*scale;
            @(posedge clk);
            for(stage=7;stage>0;stage=stage-1) expected[stage]=expected[stage-1];
            expected[0]=product[93:30];
            #1;
            if(cycle>=7 && result!==expected[7])
                $fatal(1,"Scaler mismatch at sample %0d: got %h expected %h",cycle,result,expected[7]);
        end
        $display("PASS: production DSP phase scaler preserves signed 64x32 arithmetic and eight-clock delay (4096 samples)");
        $finish;
    end
endmodule
