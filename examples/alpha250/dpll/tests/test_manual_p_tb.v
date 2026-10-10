`timescale 1 ns / 1 ps
module test_manual_p_tb;
    reg clk=0, resetn=0;
    always #2 clk=~clk;
    reg signed [16:0] freq=0;
    reg signed [31:0] phase=0;
    reg signed [15:0] i_in=4096,q_in=0;
    reg signed [31:0] cx=0,cy=166886;
    reg request=0,capture_request=0;
    reg [2:0] enabled=0;
    reg [3:0] banks=0;
    reg [8:0] command=0;
    reg [63:0] data=0;
    wire [15:0] fast,slow,baseline_fast,baseline_slow;
    wire [31:0] snapshot,status,baseline,baseline_p,baseline_integral;
    manual_p_corrector dut(clk,resetn,freq,phase,i_in,q_in,cx,cy,
        request,capture_request,enabled,banks,command,data,fast,slow,snapshot,status);
    table_corrector #(.FUSED(1),.GAIN_STAGES(2),.TAIL_GAIN_STAGES(3),.FINAL_CSA_LEVELS(2))
        reference_controller(clk,freq,phase,enabled,banks,command,data,
            baseline_fast,baseline_slow,baseline,baseline_p,baseline_integral,,);
    integer cycle=0,checked=0,k,top,address,seed=38146;
    integer impulse,fast_arrival,production_arrival,accurate_arrival;
    wire [15:0] production_fast;
    manual_p_corrector #(.GAIN_STAGES(4),.FAST_GAIN_STAGES(3),.TAIL_GAIN_STAGES(4),.I2_GAIN_STAGES(4),.FINAL_CSA_LEVELS(2),.PI_REGISTER_ADDRESS(1),.CARRY_BLOCK(0),.FAST_P_DSP(1),.PIPELINED_REFERENCE(1),.PRECOMBINE_I(1),.SELECTOR_CARRY_BLOCK(0)) production_controller(
        clk,resetn,freq,phase,i_in,q_in,cx,cy,request,capture_request,enabled,banks,
        command,data,production_fast,,,);
    reg [15:0] previous_baseline=0;
    reg previous_enable=0;
    reg [31:0] expected_p=0, delayed_p=0;
    reg signed [127:0] product;
    reg [31:0] product0=0,product1=0;
    reg signed [63:0] gain=0;
    task load_gain(input integer kind,input signed [63:0] coefficient);
        reg signed [63:0] factor;
        begin
            for(top=0;top<2;top=top+1) begin
                for(address=0;address<16;address=address+1) begin
                    @(negedge clk);
                    factor=(top && address>=8) ? address-16 : address;
                    command=9'h180 | (top<<6) | (address<<2) | kind;
                    data=coefficient*factor;
                end
            end
            @(negedge clk);command=0;banks[kind]=1;
            if(kind==0) gain=coefficient;
        end
    endtask
    always @(posedge clk) begin
        cycle=cycle+1;
        product=$signed(freq)*gain;
        product0<=product[42:11]; product1<=product0; delayed_p<=product1;
        if(!enabled[1]) expected_p<=0; else expected_p<=expected_p+delayed_p;
        #1;
        if(cycle>8) begin
            if({dut.accurate_controller.acc1,dut.accurate_controller.acc2,dut.accurate_controller.acc3,slow} !==
               {reference_controller.acc1,reference_controller.acc2,reference_controller.acc3,baseline_slow})
                $fatal(1,"Fast detector changed accurate integral state cycle=%0d",cycle);
            if(baseline_p!==expected_p) $fatal(1,"Independent accumulated P model failed cycle=%0d",cycle);
            if(baseline_p+baseline_integral !== baseline || dut.integral!==baseline_integral)
                $fatal(1,"Exact P/integral decomposition failed cycle=%0d",cycle);
            if(!request && !status[0] && enabled[1] && previous_enable && cycle<impulse &&
               fast!==previous_baseline)
                $fatal(1,"Default Accurate output changed apart from selector register");
            checked=checked+1;
        end
        previous_baseline=baseline_fast; previous_enable=enabled[1];
    end
    initial begin
        impulse=1000000;
        repeat(6) @(negedge clk);resetn=1;
        load_gain(0,64'sd134217728); // Kp=65536, phase counts visible at DAC.
        enabled=7;
        repeat(20) @(negedge clk);
        request=1;
        repeat(18) @(negedge clk);
        if(!status[0]) $fatal(1,"Manual Fast did not acknowledge");
        if(fast!==0) $fatal(1,"Static handoff changed correction");
        impulse=cycle; q_in=1000; fast_arrival=-1;production_arrival=-1;accurate_arrival=-1;
        // Model the unchanged accurate detector producing its phase difference
        // 21 clocks after filtered I/Q, versus the four-clock fast projection.
        for(k=0;k<32;k=k+1) begin
            @(negedge clk);
            if(k==20) begin freq=636; phase=636; end
            else if(k==21) freq=0;
            if(fast && fast_arrival<0) fast_arrival=cycle-impulse;
            if(production_fast && production_arrival<0) production_arrival=cycle-impulse;
            if(baseline_fast && accurate_arrival<0) accurate_arrival=cycle-impulse;
        end
        if(fast_arrival!=7 || production_arrival!=7 || accurate_arrival!=25)
            $fatal(1,"P latency expected fast=7 accurate=25 got %0d/%0d",fast_arrival,accurate_arrival);
        $display("P latency from filtered I/Q: fast=%0d clocks; modeled accurate=%0d clocks",fast_arrival,accurate_arrival);
        load_gain(1,64'sd4194304);
        load_gain(2,64'sd1099511627776);
        load_gain(3,-64'sd32768);
        // Continuous accurate states with mixed modes, gain bank changes,
        // accumulator wrap, disable/re-enable, resets and coefficient changes.
        for(k=0;k<10000;k=k+1) begin
            @(negedge clk);
            freq=$random(seed);phase=$random(seed);i_in=$random(seed);q_in=$random(seed);
            if(k%137==0) request=~request;
            enabled=(k%173==0) ? 0 : 7;
            resetn=(k%997!=0);
            if(k%211==0) begin cx=$random(seed);cy=$random(seed);end
        end
        @(negedge clk);
        $display("Manual P integration checks passed: %0d cycles; accurate I/I2/I3 states preserved",checked);
        $finish;
    end
    initial begin #1000000; $fatal(1,"Manual P timeout"); end
endmodule
