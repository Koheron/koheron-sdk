`timescale 1 ns / 1 ps
module test_p_frontend_tb;
    reg clk=0, resetn=0, acc_on=0, valid=0;
    always #2 clk=~clk;
    reg [31:0] data_a=0,data_b=0;
    wire signed [39:0] phase;
    wire signed [24:0] freq;
    wire signed [15:0] i_filtered,q_filtered;
    p_frontend_wrapper frontend(.aclk(clk),.aresetn(resetn),.acc_on(acc_on),.valid(valid),
        .data_a(data_a),.data_b(data_b),.phase(phase),.freq(freq),
        .i_filtered(i_filtered),.q_filtered(q_filtered));
    reg signed [31:0] cx=0,cy=0;
    reg request=0,capture_request=0;
    reg [2:0] enabled=0;
    reg [3:0] banks=0;
    reg [8:0] command=0;
    reg [63:0] data=0;
    wire [15:0] fast,slow,baseline_fast,baseline_slow;
    wire [31:0] snapshot,status;
    manual_p_corrector #(.GAIN_STAGES(4),.FAST_GAIN_STAGES(3),.TAIL_GAIN_STAGES(4),.FINAL_CSA_LEVELS(2),.CARRY_BLOCK(0),.FAST_P_DSP(1),.PIPELINED_REFERENCE(1),.PRECOMBINE_I(1),.SELECTOR_CARRY_BLOCK(0),.FREQ_WIDTH(25),.PHASE_WIDTH(40),.PHASE_FRAC(8)) controller(clk,resetn,freq,phase,i_filtered,q_filtered,cx,cy,
        request,capture_request,enabled,banks,command,data,fast,slow,snapshot,status);
    table_corrector #(.FUSED(1),.GAIN_STAGES(4),.TAIL_GAIN_STAGES(4),.FINAL_CSA_LEVELS(2),.CARRY_BLOCK(0),.FREQ_WIDTH(25),.PHASE_WIDTH(40),.PHASE_FRAC(8))
        baseline(clk,freq,phase,enabled,banks,command,data,baseline_fast,baseline_slow,,,,,);
    reg signed [15:0] accurate_delayed=0;
    integer sample=0,cycle=0,k,top,address,fast_arrival,accurate_arrival,disturbance;
    integer adc,cosine,sine,reference_i,reference_q;
    integer fast_before,accurate_before,previous_output;
    reg previous_mode;
    real perturbation=0,carrier,norm;
    localparam real PI=3.14159265358979323846;
    always @(negedge clk) begin
        carrier=sample*PI/4;
        adc=$rtoi(8000*$cos(carrier+PI/6+perturbation));
        cosine=$rtoi(32767*$cos(carrier));sine=$rtoi(32767*$sin(carrier));
        data_a={16'b0,adc[15:0]};data_b={sine[15:0],cosine[15:0]};
        sample=sample+1;
    end
    always @(posedge clk) begin
        cycle=cycle+1; accurate_delayed<=baseline_fast;
        previous_output=$signed(fast);previous_mode=status[0];
        #1;
        if(status[0]!=previous_mode && $signed(fast)!=previous_output)
            $fatal(1,"Generated-front-end handoff jumped");
        if(slow!==baseline_slow || controller.accurate_controller.acc1!==baseline.acc1 ||
           controller.accurate_controller.acc2!==baseline.acc2 || controller.accurate_controller.acc3!==baseline.acc3)
            $fatal(1,"Generated front end changed accurate integral states");
    end
    initial begin
        repeat(5) @(negedge clk);resetn=1;valid=1;
        repeat(100) @(negedge clk);acc_on=1;
        for(top=0;top<2;top=top+1) begin
            for(address=0;address<16;address=address+1) begin
                @(negedge clk);command=9'h180|(top<<6)|(address<<2);
                data=64'sd134217728*((top && address>=8) ? address-16 : address);
            end
        end
        @(negedge clk);command=0;banks=1;enabled=7;
        repeat(50) @(negedge clk);capture_request=1;
        wait(status[2]); @(negedge clk);
        reference_i=$signed(snapshot[15:0]);reference_q=$signed(snapshot[31:16]);
        norm=1.0*reference_i*reference_i+1.0*reference_q*reference_q;
        if(norm<4096) $fatal(1,"Invalid front-end calibration");
        cx=-reference_q*8192.0/PI*262144.0/norm;
        cy=reference_i*8192.0/PI*262144.0/norm;
        repeat(12) @(negedge clk); request=1;
        wait(status[0]);repeat(12) @(negedge clk);
        if(!status[1]) $fatal(1,"Captured lock reference outside fast range");
        fast_before=$signed(fast);accurate_before=accurate_delayed;
        disturbance=cycle;fast_arrival=-1;accurate_arrival=-1;
        #0.1;perturbation=PI/18;
        for(k=0;k<160;k=k+1) begin
            @(posedge clk);#1.1;
            if(($signed(fast)-fast_before>100 || $signed(fast)-fast_before < -100) && fast_arrival<0)
                fast_arrival=cycle-disturbance;
            if((accurate_delayed-accurate_before>100 || accurate_delayed-accurate_before < -100) && accurate_arrival<0)
                accurate_arrival=cycle-disturbance;
        end
        if(fast_arrival<0 || accurate_arrival<0 || accurate_arrival-fast_arrival<18)
            $fatal(1,"No front-end latency saving fast=%0d accurate=%0d",fast_arrival,accurate_arrival);
        // At 10 degrees, sin differs from angle by only 2.3 phase counts.
        if(($signed(fast)-fast_before)-(accurate_delayed-accurate_before)>6 ||
           ($signed(fast)-fast_before)-(accurate_delayed-accurate_before)<-6)
            $fatal(1,"Fast/accurate response scale mismatch");
        $display("Generated mixer/filter/extractor P response: fast=%0d clocks accurate=%0d clocks saving=%0d clocks",fast_arrival,accurate_arrival,accurate_arrival-fast_arrival);
        @(negedge clk);request=0;wait(!status[0]);repeat(20) @(negedge clk);
        $display("P front-end checks passed: 31.25 MHz carrier, arbitrary lock phase, 10-degree step, both handoffs");
        $finish;
    end
    initial begin #20000; $fatal(1,"P front-end timeout"); end
endmodule
