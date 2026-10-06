`timescale 1 ns / 1 ps
module constant_gain_test #(parameter integer FRACTION_BITS=11);
    reg clk=0;
    always #2 clk=~clk;
    reg signed [47:0] a=0;
    reg signed [12:0] coefficient=0;
    reg [4:0] octave=0;
    reg [4:0] selection;
    integer magnitude;
    always @* begin
        magnitude=coefficient<0 ? -coefficient : coefficient;
        if(FRACTION_BITS==7) begin
        case(magnitude)
            2048: selection=0; 2176: selection=1; 2304: selection=2;
            2432: selection=3; 2592: selection=4; 2736: selection=5;
            2896: selection=6; 3072: selection=7; 3264: selection=8;
            3456: selection=9; 3648: selection=10; 3872: selection=11;
            default: selection=24;
        endcase
        end else begin
        case(magnitude)
            2048: selection=0; 2170: selection=1; 2299: selection=2;
            2435: selection=3; 2580: selection=4; 2734: selection=5;
            2896: selection=6; 3069: selection=7; 3251: selection=8;
            3444: selection=9; 3649: selection=10; 3866: selection=11;
            default: selection=24;
        endcase
        end
        if(coefficient<0) selection=selection+12;
    end
    wire [159:0] result [2:3];
    reg [159:0] expected_in=0, expected_pipe[0:2];
    reg [225:0] vectors[0:99999];
    reg [1023:0] vector_path;
    integer count, cycle=0, checked=0, i, j;
    genvar stages;
    generate for(stages=2; stages<=3; stages=stages+1) begin : variant
        constant_gain #(.A_WIDTH(17), .OUTPUT_WIDTH(32), .PIPE_STAGES(stages), .FRACTION_BITS(FRACTION_BITS))
            gp(clk, a[16:0], selection, octave, result[stages][31:0]);
        constant_gain #(.A_WIDTH(32), .OUTPUT_LOW(16), .OUTPUT_WIDTH(32), .PIPE_STAGES(stages), .FRACTION_BITS(FRACTION_BITS))
            gpi(clk, a[31:0], selection, octave, result[stages][63:32]);
        constant_gain #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32), .PIPE_STAGES(stages), .FRACTION_BITS(FRACTION_BITS))
            gi2(clk, a, selection, octave, result[stages][95:64]);
        constant_gain #(.A_WIDTH(32), .OUTPUT_WIDTH(64), .PIPE_STAGES(stages), .FRACTION_BITS(FRACTION_BITS))
            gi3(clk, a[31:0], selection, octave, result[stages][159:96]);
    end endgenerate
    always @(posedge clk) begin
        expected_pipe[0]<=expected_in;
        for(j=1;j<3;j=j+1) expected_pipe[j]<=expected_pipe[j-1];
        cycle=cycle+1;
        #1;
        if(cycle>10) begin
            if(result[2] !== expected_pipe[1] || result[3] !== expected_pipe[2])
                $fatal(1,"Constant gain mismatch at cycle %0d: %h / %h expected %h / %h",
                    cycle,result[2],result[3],expected_pipe[1],expected_pipe[2]);
            checked=checked+1;
        end
    end
    initial begin
        if(!$value$plusargs("vectors=%s",vector_path) || !$value$plusargs("count=%d",count))
            $fatal(1,"Expected vectors and count plusargs");
        $readmemh(vector_path,vectors,0,count-1);
        repeat(10) @(negedge clk);
        for(i=0;i<count;i=i+1) begin
            @(negedge clk);
            {a,coefficient,octave,expected_in}=vectors[i];
        end
        repeat(4) @(negedge clk);
        $display("Constant gain checks passed: %0d cycles, Q1.%0d coefficients and two/three-clock latency",checked,FRACTION_BITS);
        $finish;
    end
endmodule
module constant_gain_test_7;
    constant_gain_test #(.FRACTION_BITS(7)) test();
endmodule
