`timescale 1ns/1fs
module test_multirate_frontend_tb;
    reg clk=0, reference_clk=0, resetn=0, feed=0;
    always #1.041666666666667 clk=~clk;
    always @(posedge clk) reference_clk<=~reference_clk;
    reg valid_in=0;
    reg [31:0] data_a=0, data_b=0;
    reg [7:0] random_round=0;
    wire [15:0] i_filtered, q_filtered;
    wire valid_out, reference_valid;
    wire [47:0] reference_data;
    frontend_test_wrapper dut(.*);

    reg request_fast=0, capture_request=0, control_test=0, phase_step=0;
    reg [2:0] enabled=0;
    reg [3:0] banks=0;
    reg [8:0] command=0;
    reg [63:0] gain_data=0;
    reg signed [31:0] cx=0,cy=0;
    wire [15:0] correction,slow,rf_output;
    wire [31:0] snapshot,status;
    manual_p_corrector #(.GAIN_STAGES(6),.FAST_GAIN_STAGES(4),.TAIL_GAIN_STAGES(6),
        .I2_GAIN_STAGES(6),.FINAL_CSA_LEVELS(2),.FAST_P_DSP(1),.PIPELINED_REFERENCE(1),
        .PRECOMBINE_I(1),.FREQ_WIDTH(25),.PHASE_WIDTH(40),.PHASE_FRAC(8),.SAMPLE_INTERVAL(2),
        .CARRY_BLOCK(0),.GAIN_CARRY_BLOCK(16),.SUM_CARRY_BLOCK(16),
        .FAST_CARRY_BLOCK(0),.SELECTOR_CARRY_BLOCK(0)) controller(
        clk,resetn,25'sd0,40'sd0,i_filtered,q_filtered,cx,cy,
        request_fast,capture_request,enabled,banks,command,gain_data,
        correction,slow,snapshot,status,valid_out);
    latched_mux #(.WIDTH(16),.N_INPUTS(2),.SEL_WIDTH(1),.OUTPUT_STAGES(2))
        dac_mux(clk,1'b1,{16'b0,correction},1'b0,rf_output);

    integer cycle=0, input_count=0, output_count=0, reference_count=0;
    integer pattern=0, checks=0, latency=-1;
    integer accepted_cycle[0:4095];
    reg [31:0] actual[0:4095], expected[0:4095];
    reg signed [15:0] hi0=0,hi1=0,hi2=0,hq0=0,hq1=0,hq2=0;
    reg signed [31:0] sum_i,sum_q;
    integer random_state=32'h31415926;
    integer next_random;
    integer step_cycle=-1;
    integer integrate_count=0,fast_integrate_count=0;

    // Inputs change even during invalid processing edges. Only complete valid
    // epochs may reach the multiplier, filter history or reference capture.
    always @(negedge clk) begin
        next_random=$random(random_state);
        data_a=$random(random_state);
        data_b=$random(random_state);
        random_round=$random(random_state);
        if(control_test) begin
            data_a=32'd8000;
            data_b={phase_step ? 16'd5689 : 16'd0,16'd32767};
        end
        valid_in=feed && resetn && !reference_clk &&
                 (pattern==0 || (next_random & 7)!=0);
    end
    always @(posedge clk) begin
        cycle=cycle+1;
        if(resetn && control_test) begin
            if(controller.accurate_controller.integrate) integrate_count=integrate_count+1;
            if(controller.fast_integrate) fast_integrate_count=fast_integrate_count+1;
        end
        if(resetn && valid_in) begin
            if(control_test && data_b[31:16]!=0 && step_cycle<0) step_cycle=cycle;
            accepted_cycle[input_count]=cycle;
            input_count=input_count+1;
        end
        #0.01;
        if(resetn && valid_out) begin
            if(output_count>=input_count) $fatal(1,"Output without an accepted sample");
            if($isunknown({i_filtered,q_filtered})) $fatal(1,"Unknown filter output");
            actual[output_count]={q_filtered,i_filtered};
            if(latency<0) latency=cycle-accepted_cycle[output_count]+1;
            if(cycle-accepted_cycle[output_count]+1!=7)
                $fatal(1,"Expected seven processing clocks, observed %0d",cycle-accepted_cycle[output_count]+1);
            output_count=output_count+1;
        end
    end
    // Independent four-sample arithmetic model driven by an identical vendor
    // multiplier at the sample clock, including its exact random rounding.
    always @(posedge reference_clk) begin
        #0.01;
        if(resetn && reference_valid) begin
            sum_i=$signed(reference_data[23:8]);
            sum_q=$signed(reference_data[47:32]);
            sum_i=sum_i+hi0+hi1+hi2;
            sum_q=sum_q+hq0+hq1+hq2;
            expected[reference_count]={sum_q[17:2],sum_i[17:2]};
            hi2=hi1;hi1=hi0;hi0=reference_data[23:8];
            hq2=hq1;hq1=hq0;hq0=reference_data[47:32];
            reference_count=reference_count+1;
        end
    end
    task run_epoch(input integer mode);
        integer k;
        begin
            @(negedge clk); resetn=0;feed=0;pattern=mode;
            repeat(12) @(negedge clk);
            input_count=0;output_count=0;reference_count=0;
            hi0=0;hi1=0;hi2=0;hq0=0;hq1=0;hq2=0;
            resetn=1;
            repeat(4) @(negedge clk);
            feed=1;
            repeat(2200) @(negedge clk);
            feed=0;
            repeat(32) @(negedge clk);
            if(input_count<800 || output_count!=input_count || reference_count!=input_count)
                $fatal(1,"Lost/duplicated samples: input=%0d fast=%0d reference=%0d",input_count,output_count,reference_count);
            for(k=0;k<input_count;k=k+1) begin
                if(actual[k]!==expected[k])
                    $fatal(1,"Sample %0d changed: actual=%h expected=%h",k,actual[k],expected[k]);
                checks=checks+1;
            end
        end
    endtask
    task gain(input integer kind,input signed [63:0] value);
        integer top,address;
        begin
            for(top=0;top<2;top=top+1)
                for(address=0;address<16;address=address+1) begin
                    @(negedge clk); command=9'h180|(top<<6)|(address<<2)|kind;
                    gain_data=value*((top && address>=8) ? address-16 : address);
                end
            @(negedge clk);command=0;banks[kind]=1;
        end
    endtask
    task control_latency;
        integer k,before_rf,response_cycle,reference_i,reference_q,accepted_before,integrated_before;
        real norm;
        begin
            @(negedge clk); resetn=0;feed=0;control_test=1;pattern=0;
            repeat(12) @(negedge clk);
            input_count=0;output_count=0;reference_count=0;
            hi0=0;hi1=0;hi2=0;hq0=0;hq1=0;hq2=0;
            resetn=1;feed=1;
            for(k=0;k<4;k=k+1) gain(k,k==0 ? 134217728 : (k==1 ? 2048 : 0));
            enabled=3'b011;
            repeat(100) @(negedge clk);
            accepted_before=input_count;
            integrated_before=integrate_count;
            repeat(200) @(negedge clk);
            if(input_count-accepted_before!=100 ||
               integrate_count-integrated_before!=100)
                $fatal(1,"Frontend sample cadence changed the background integrator");
            capture_request=1;
            wait(status[2]);
            reference_i=$signed(snapshot[15:0]);reference_q=$signed(snapshot[31:16]);
            norm=1.0*reference_i*reference_i+1.0*reference_q*reference_q;
            if(norm<4096) $fatal(1,"Invalid frontend reference snapshot");
            cx=-reference_q*8192.0/3.141592653589793*262144.0/norm;
            cy=reference_i*8192.0/3.141592653589793*262144.0/norm;
            repeat(32) @(negedge clk);
            request_fast=1;
            wait(status[0]);
            repeat(32) @(negedge clk);
            integrated_before=fast_integrate_count;
            repeat(200) @(negedge clk);
            if(fast_integrate_count-integrated_before!=100)
                $fatal(1,"Frontend sample cadence changed the Fast integrator");
            before_rf=$signed(rf_output);
            @(posedge reference_clk);#0.02;phase_step=1;
            response_cycle=-1;
            for(k=0;k<80;k=k+1) begin
                @(posedge clk);#0.02;
                if(response_cycle<0 && ($signed(rf_output)-before_rf>32 ||
                   $signed(rf_output)-before_rf < -32)) response_cycle=cycle;
            end
            if(step_cycle<0 || response_cycle-step_cycle+1!=17)
                $fatal(1,"Expected 17 processing clocks through Fast P and DAC mux: input=%0d output=%0d",step_cycle,response_cycle);
            $display("Multirate Fast P input epoch through DAC mux: 17 processing clocks / 35.417 ns, four distinct-sample taps retained");
        end
    endtask
    initial begin
        run_epoch(0);
        run_epoch(1);
        run_epoch(1);
        control_latency;
        $display("Multirate frontend checks passed: %0d bit-exact complex samples, continuous/gapped epochs, reset recovery, seven clocks at 480 MHz",checks);
        $finish;
    end
    initial begin #50000; $fatal(1,"Multirate frontend timeout"); end
endmodule
