`timescale 1ns/1ps
module dpll_dsp_primitives_tb;
    reg clk=0;
    always #2 clk=~clk;
    reg signed [24:0] coefficient=0;
    reg signed [16:0] sample=0;
    reg signed [29:0] a=0;
    reg signed [17:0] b=0;
    reg signed [47:0] c=0;
    wire [47:0] product, subtract_mac, add_mac;
    dpll_p_product gain(clk,coefficient,sample,product);
    phase_residual_mac #(.SUBTRACT(1),.REGISTER_RESULT(1),.REGISTER_A(1),.REGISTER_B(1),.REGISTER_C(1))
        interpolation(clk,a,b,c,subtract_mac);
    phase_residual_mac #(.REGISTER_RESULT(0),.REGISTER_MULTIPLIER(1),.REGISTER_A(1),.REGISTER_C(1))
        final_angle(clk,a,b,c,add_mac);
    reg signed [47:0] product_input=0, product_expected=0;
    reg signed [24:0] a_input=0;
    reg signed [17:0] b_input=0;
    reg signed [47:0] c_input=0, multiply_input=0;
    reg signed [47:0] sub_expected=0;
    integer cycle=0, checked=0;
    // Independent models retain exactly the production A/B/C/M/P boundaries.
    always @(posedge clk) begin
        product_expected <= product_input;
        product_input <= coefficient*sample;
        sub_expected <= c_input-a_input*b_input;
        multiply_input <= a_input*b;
        a_input <= a[24:0]; b_input <= b; c_input <= c;
        #0.1;
        if(cycle>30) begin
            if(product!==product_expected) $fatal(1,"Signed gain/latency mismatch at %0d",cycle);
            if(subtract_mac!==sub_expected) $fatal(1,"Subtract MAC/latency mismatch at %0d",cycle);
            if(add_mac!==multiply_input+c_input) $fatal(1,"Add MAC/latency mismatch at %0d",cycle);
            checked=checked+1;
        end
        cycle=cycle+1;
    end
    initial begin
        // UNISIM startup reset is active for the first 100 ns.
        #120;
        for(integer i=0;i<10000;i=i+1) begin
            @(negedge clk);
            coefficient=$random; sample=$random;
            a=$random; b=$random; c={$random,$random};
            if(i<16) begin
                coefficient=(i&1) ? 25'sh1000000 : 25'sh0ffffff;
                sample=(i&2) ? 17'sh10000 : 17'sh0ffff;
                a=(i&4) ? -30'sd16777216 : 30'sd16777215;
                b=(i&8) ? -18'sd131072 : 18'sd131071;
            end
        end
        repeat(4) @(negedge clk);
        $display("DSP gain/MAC checks passed: %0d cycles",checked);
        $finish;
    end
endmodule
