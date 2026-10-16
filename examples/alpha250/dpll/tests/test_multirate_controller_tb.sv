`timescale 1ns/1ps
module test_multirate_controller_tb #(
    parameter integer CARRY_BLOCK=0,
    parameter integer GAIN_CARRY_BLOCK=CARRY_BLOCK, SELECTOR_CARRY_BLOCK=CARRY_BLOCK,
    parameter real CLOCK_HALF_PERIOD=1.25
);
    reg clk=0, resetn=0, sample_valid=0;
    always #(CLOCK_HALF_PERIOD) clk=~clk;
    always @(negedge clk) sample_valid<=~sample_valid;
    reg request=0, capture_request=0;
    reg [2:0] enabled=0;
    reg [3:0] banks=0;
    reg [8:0] command=0;
    reg [63:0] data=0;
    reg signed [15:0] quadrature=0;
    wire [15:0] correction,slow;
    wire [31:0] snapshot,status;
    manual_p_corrector #(.GAIN_STAGES(5),.FAST_GAIN_STAGES(4),.TAIL_GAIN_STAGES(5),
        .I2_GAIN_STAGES(5),.FINAL_CSA_LEVELS(2),.FAST_P_DSP(1),.PIPELINED_REFERENCE(1),
        .PRECOMBINE_I(1),.FREQ_WIDTH(25),.PHASE_WIDTH(40),.PHASE_FRAC(8),.SAMPLE_INTERVAL(2),
        .CARRY_BLOCK(CARRY_BLOCK),.GAIN_CARRY_BLOCK(GAIN_CARRY_BLOCK),.SELECTOR_CARRY_BLOCK(SELECTOR_CARRY_BLOCK)) dut(
        clk,resetn,25'sd0,40'sd16777216,16'sd4096,quadrature,32'sd0,32'sd166886,
        request,capture_request,enabled,banks,command,data,correction,slow,snapshot,status,sample_valid);
    integer kind,top,address,k,fast_samples,accurate_samples;
    reg [31:0] before_accurate,before_fast;
    reg [47:0] before_first;
    task gain(input integer gain_kind,input signed [63:0] value);
        begin
            for(top=0;top<2;top=top+1)
                for(address=0;address<16;address=address+1) begin
                    @(negedge clk); command=9'h180|(top<<6)|(address<<2)|gain_kind;
                    data=value*((top && address>=8) ? address-16 : address);
                end
            @(negedge clk); command=0; banks[gain_kind]=1;
        end
    endtask
    // Exercise the fully registered native product separately from zero P gain.
    reg signed [24:0] coefficient=0;
    reg signed [16:0] sample=0;
    wire [47:0] product;
    reg signed [47:0] product0=0,product1=0,product2=0;
    dpll_p_product #(.MULTIPLIER_REGISTER(1)) multiply(clk,coefficient,sample,product);
    integer product_checks=0;
    always @(negedge clk) begin coefficient<=$random; sample<=$random; end
    always @(posedge clk) begin
        product0<=coefficient*sample;
        product1<=product0;
        product2<=product1;
        #0.4;
        if($time>200) begin
            if(product !== product2) $fatal(1,"Registered product arithmetic/latency mismatch");
            product_checks=product_checks+1;
        end
    end
    initial begin
        repeat(100) @(negedge clk);
        resetn=1;
        for(kind=0;kind<4;kind=kind+1) gain(kind,kind==1 ? 2048 : 0);
        enabled=3'b011;
        repeat(100) @(negedge clk);
        before_accurate=dut.accurate_i;
        before_first=dut.accurate_controller.acc1;
        accurate_samples=0;
        for(k=0;k<200;k=k+1) begin
            @(posedge clk);
            if(dut.accurate_controller.integrate) accurate_samples=accurate_samples+1;
            #0.4;
        end
        if(accurate_samples!=100 || dut.accurate_i-before_accurate!=100 ||
           dut.accurate_controller.acc1-before_first!=100)
            $fatal(1,"Accurate integrators must update once per ADC sample: %d/%d",accurate_samples,dut.accurate_i-before_accurate);
        @(negedge clk); capture_request=1;
        k=0;
        while(!status[2] && k<200) begin @(negedge clk); k=k+1; end
        if(!status[2] || k<126 || snapshot!=32'h00001000 || dut.reference_phase!=40'sd16777216)
            $fatal(1,"Calibration must average 64 distinct ADC samples: %d clocks",k);
        request=1;
        repeat(100) @(negedge clk);
        if(!status[0]) $fatal(1,"Fast mode did not activate");
        before_fast=dut.fast_i_acc;
        before_accurate=dut.accurate_i;
        fast_samples=0;
        for(k=0;k<200;k=k+1) begin
            @(posedge clk);
            if(dut.fast_integrate) fast_samples=fast_samples+1;
            #0.4;
        end
        if(fast_samples!=100 || dut.fast_i_acc-before_fast!=100 || dut.accurate_i-before_accurate!=100)
            $fatal(1,"Fast/background I gains changed with processing rate: %d/%d",fast_samples,dut.fast_i_acc-before_fast);
        gain(1,0);
        gain(0,134217728);
        repeat(100) @(negedge clk);
        before_fast=dut.selected;
        quadrature=128;
        k=0;
        while(dut.selected==before_fast && k<30) begin @(posedge clk); #0.4; k=k+1; end
        if(k!=8) $fatal(1,"Unexpected Fast P latency from filtered IQ: %d clocks",k);
        $display("Multirate Fast P latency from filtered IQ through selector: 8 processing clocks / %0.3f ns",16*CLOCK_HALF_PERIOD);
        if(product_checks<700) $fatal(1,"Insufficient randomized DSP checks");
        $display("Multirate controller checks passed: %0.3f MHz processing, %0.3f MS/s states, 64-sample capture, registered DSP product",500/CLOCK_HALF_PERIOD,250/CLOCK_HALF_PERIOD);
        $finish;
    end
    initial begin #20000; $fatal(1,"Multirate controller test timed out"); end
endmodule
