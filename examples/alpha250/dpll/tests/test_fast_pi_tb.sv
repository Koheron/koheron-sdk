`timescale 1ns/1ps
module test_fast_pi_tb;
    reg clk=0,resetn=0;
    always #2 clk=~clk;
    reg signed [24:0] freq=0;
    reg signed [39:0] phase=-40'sd198784; // -776.5 legacy phase counts.
    reg signed [15:0] iq=0;
    reg request=0,capture_request=0;
    reg [2:0] enabled=0;
    reg [3:0] banks=0;
    reg [8:0] command=0;
    reg [63:0] data=0;
    wire [15:0] output_word,slow,baseline_slow;
    wire [31:0] status,snapshot,accurate,baseline_i,baseline_higher;
    manual_p_corrector #(.GAIN_STAGES(4),.FAST_GAIN_STAGES(3),.TAIL_GAIN_STAGES(4),.FINAL_CSA_LEVELS(2),.CARRY_BLOCK(0),.FAST_P_DSP(1),.PIPELINED_REFERENCE(1),.PRECOMBINE_I(1),.SELECTOR_CARRY_BLOCK(0),.FREQ_WIDTH(25),.PHASE_WIDTH(40),.PHASE_FRAC(8)) dut(
        clk,resetn,freq,phase,16'sd4096,iq,32'sd0,32'sd166886,
        request,capture_request,enabled,banks,command,data,output_word,slow,snapshot,status);
    table_corrector #(.FUSED(1),.GAIN_STAGES(4),.TAIL_GAIN_STAGES(4),
        .FINAL_CSA_LEVELS(2),.CARRY_BLOCK(0),.FREQ_WIDTH(25),.PHASE_WIDTH(40),.PHASE_FRAC(8)) baseline(
        clk,freq,phase,enabled,banks,command,data,,baseline_slow,accurate,,,
        baseline_i,baseline_higher);
    integer top,address,k,i_arrival;
    reg [31:0] previous_i,previous_output;
    reg previous_mode;
    task gain(input integer kind,input signed [63:0] value);
        begin
            for(top=0;top<2;top=top+1)
                for(address=0;address<16;address=address+1) begin
                    @(negedge clk); command=9'h180|(top<<6)|(address<<2)|kind;
                    data=value*((top && address>=8) ? address-16 : address);
                end
            @(negedge clk);command=0;banks[kind]=1;
        end
    endtask
    always @(posedge clk) begin
        previous_mode=status[0];previous_output=dut.selected;
        #1;
        if(status[0]!=previous_mode && dut.selected!==previous_output)
            $fatal(1,"PI handoff changed an output sample");
        if(dut.accurate_controller.acc1!==baseline.acc1 ||
           dut.accurate_controller.acc2!==baseline.acc2 || slow!==baseline_slow ||
           dut.higher!==baseline_higher)
            $fatal(1,"Fast PI modified accurate higher-order state");
        if(baseline.i_correction+baseline.higher_correction!==baseline.integral_correction)
            $fatal(1,"I/higher decomposition is not exact");
    end
    initial begin
        repeat(4) @(negedge clk);resetn=1;
        gain(1,64'sd134217728); // K_i=65536: increment=floor(phase/256).
        gain(2,64'sd2199023255552);
        gain(3,-64'sd32768);
        enabled=7;
        repeat(20) @(negedge clk);capture_request=1;
        wait(status[2]);@(negedge clk);
        if(dut.reference_phase!==phase) $fatal(1,"Capture lost accurate phase reference");
        request=1;wait(status[0]);repeat(12) @(negedge clk);
        // A nonzero captured error must keep integrating, rather than being
        // silently replaced with zero by the calibrated projection.
        previous_i=dut.fast_i_acc;
        repeat(100) @(negedge clk);
        if(dut.fast_i_acc!==previous_i-32'd77700)
            $fatal(1,"Fast I changed the phase origin or fractional scaling");
        iq=1000;i_arrival=-1;
        for(k=1;k<=12;k=k+1) begin
            previous_i=dut.fast_i_acc;
            @(negedge clk);
            if(dut.fast_i_acc-previous_i===32'hffffff73 && i_arrival<0) i_arrival=k;
        end
        if(i_arrival!=11) $fatal(1,"Fast I accumulator latency expected 11 got %0d",i_arrival);
        $display("Fast I latency: eleven clocks to accumulator, thirteen through selector; Fast P seven clocks");
        if(dut.fast_phase!==17'sd636) $fatal(1,"Unexpected projection");
        previous_i=dut.fast_i_acc;
        repeat(100) @(negedge clk);
        if(dut.fast_i_acc!==previous_i-32'd14100)
            $fatal(1,"Fast I did not respond independently of accurate phase");
        request=0;wait(!status[0]);repeat(5) @(negedge clk);
        // Averaging phase across its modular wrap must retain a coherent origin.
        phase=40'sh7ffffffe00;capture_request=0;
        @(negedge clk);
        for(k=0;k<64;k=k+1) begin
            phase=40'sh7ffffffe00+(k*256);
            @(negedge clk);
        end
        wait(!status[2]);@(negedge clk);
        if(dut.reference_phase!==(40'sh7ffffffe00+40'sd8064))
            $fatal(1,"Captured phase average failed at modular wrap");
        phase=0;freq=-25'sd128;
        gain(0,64'sd134217728);
        repeat(12) @(negedge clk);
        if(dut.accurate_controller.p!==32'hffff8000)
            $fatal(1,"Accurate P lost signed fractional phase precision");
        freq=25'sd383;repeat(12) @(negedge clk);
        if(dut.accurate_controller.p!==32'd98048)
            $fatal(1,"Accurate P input scaling changed");
        $display("Fast PI checks passed: accurate anchoring, fractional scaling, I response, accurate tails and handoffs");
        $finish;
    end
    initial begin #20000;$fatal(1,"Fast PI timeout");end
endmodule
