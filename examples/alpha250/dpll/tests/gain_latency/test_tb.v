`timescale 1 ns / 1 ps
module geometric_gain_test;
    reg clk = 0;
    always #2 clk = ~clk;
    reg signed [47:0] a = 0;
    reg signed [12:0] coefficient = 0;
    wire signed [12:0] negative_coefficient = -coefficient;
    reg [4:0] octave = 0;
    wire [159:0] result [2:7];
    reg [159:0] expected_in = 0;
    reg [159:0] expected_pipe [0:2];
    wire [159:0] production_result;
    wire signed [31:0] power_of_two = 32'sd1 <<< octave;
    wire signed [31:0] old_gain = (coefficient == 0) ? 0 :
        (coefficient[12] ? -power_of_two : power_of_two);
    wire compatible = coefficient == 0 || coefficient == -2048 ||
        (coefficient == 2048 && octave < 31);
    reg [2:0] compatible_pipe = 0;
    integer compatibility_checked = 0;
    reg [225:0] vectors [0:99999];
    integer count, cycle = 0, checked = 0, i, j;
    reg [1023:0] vector_path;
    gain_multiplier #(.A_WIDTH(17), .OUTPUT_WIDTH(32))
        old_p(clk, a[16:0], old_gain, production_result[31:0]);
    gain_multiplier #(.A_WIDTH(32), .OUTPUT_LOW(16), .OUTPUT_WIDTH(32))
        old_pi(clk, a[31:0], old_gain, production_result[63:32]);
    gain_multiplier #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32))
        old_i2(clk, a, old_gain, production_result[95:64]);
    gain_multiplier #(.A_WIDTH(32), .OUTPUT_WIDTH(64))
        old_i3(clk, a[31:0], old_gain, production_result[159:96]);
    genvar latency;
    generate for (latency = 2; latency <= 7; latency = latency + 1) begin : implementation
        localparam STAGES = (latency >= 4) ? 2+(latency%2) : latency;
        localparam DSP_OUTPUT_REG = (latency >= 4) ? 1 : 0;
        localparam DSP_SIGN_CORRECTION = (latency >= 6) ? 1 : 0;
        geometric_gain #(.A_WIDTH(17), .OUTPUT_WIDTH(32), .PIPE_STAGES(STAGES), .DSP_OUTPUT_REG(DSP_OUTPUT_REG), .DSP_SIGN_CORRECTION(DSP_SIGN_CORRECTION))
            gp(clk, a[16:0], coefficient, negative_coefficient, octave, result[latency][31:0]);
        geometric_gain #(.A_WIDTH(32), .OUTPUT_LOW(16), .OUTPUT_WIDTH(32), .PIPE_STAGES(STAGES), .DSP_OUTPUT_REG(DSP_OUTPUT_REG), .DSP_SIGN_CORRECTION(DSP_SIGN_CORRECTION))
            gpi(clk, a[31:0], coefficient, negative_coefficient, octave, result[latency][63:32]);
        geometric_gain #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32), .PIPE_STAGES(STAGES), .DSP_OUTPUT_REG(DSP_OUTPUT_REG), .DSP_SIGN_CORRECTION(DSP_SIGN_CORRECTION))
            gi2(clk, a, coefficient, negative_coefficient, octave, result[latency][95:64]);
        geometric_gain #(.A_WIDTH(32), .OUTPUT_WIDTH(64), .PIPE_STAGES(STAGES), .DSP_OUTPUT_REG(DSP_OUTPUT_REG), .DSP_SIGN_CORRECTION(DSP_SIGN_CORRECTION))
            gi3(clk, a[31:0], coefficient, negative_coefficient, octave, result[latency][159:96]);
    end endgenerate
    always @(posedge clk) begin
        expected_pipe[0] <= expected_in;
        compatible_pipe <= {compatible_pipe[1:0], compatible};
        for (j = 1; j < 3; j = j + 1) expected_pipe[j] <= expected_pipe[j-1];
        cycle = cycle + 1;
        #1;
        if (cycle > 10) begin
            if (result[2] !== expected_pipe[1] || result[3] !== expected_pipe[2] ||
                result[4] !== expected_pipe[1] || result[5] !== expected_pipe[2] ||
                result[6] !== expected_pipe[1] || result[7] !== expected_pipe[2])
                $fatal(1, "Geometric gain mismatch at cycle %0d: two=%h three=%h expected=%h/%h",
                       cycle, result[2], result[3], expected_pipe[1], expected_pipe[2]);
            checked = checked + 1;
            if (compatible_pipe[2]) begin
                if (production_result !== expected_pipe[2])
                    $fatal(1, "Production power-of-two compatibility mismatch at cycle %0d", cycle);
                compatibility_checked = compatibility_checked + 1;
            end
        end
    end
    initial begin
        if (!$value$plusargs("vectors=%s", vector_path) || !$value$plusargs("count=%d", count))
            $fatal(1, "Expected vectors and count plusargs");
        $readmemh(vector_path, vectors, 0, count-1);
        repeat (10) @(negedge clk);
        for (i = 0; i < count; i = i + 1) begin
            @(negedge clk);
            {a, coefficient, octave, expected_in} = vectors[i];
        end
        repeat (4) @(negedge clk);
        $display("Geometric gain checks passed: %0d cycles, all four slices, exact two/three-clock latency", checked);
        $display("Production compatibility checks passed: %0d zero/power-of-two cases", compatibility_checked);
        $finish;
    end
endmodule
