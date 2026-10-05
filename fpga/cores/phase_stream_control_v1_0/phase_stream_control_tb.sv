`timescale 1ns/1ps
module phase_stream_control_tb;
    reg clk=0; always #2.5 clk=~clk;
    reg resetn=0,ready=0,config_ready=0;
    reg [15:0] requested_rate=0;
    wire valid,config_valid,filter_resetn;
    wire [15:0] config_rate;
    reg [3:0] requested_bits=0;
    reg requested_run=0, requested_epoch=0, upstream_overflow=0;
    reg expected_gap=0;
    wire sample_gap,overflow;
    wire [3:0] active_bits;
    phase_stream_control dut(.aclk(clk),.aresetn(resetn),.*,
        .data_ready(ready),.data_valid(valid));
    integer tick,total=0,commits=0,reset_width=0,completed_resets=0;
    reg was_reset=1, observed_reset=1;
    initial begin
        for(tick=0;tick<20000;tick=tick+1) begin
            @(negedge clk);
            ready=(tick%113)<83;
            config_ready=(tick%11)<7;
            if(tick==8) resetn=1;
            if(tick==10) requested_rate=20;
            if(tick==50) requested_run=1;
            if(tick==6001) requested_rate=67;
            if(tick==6007) requested_rate=100;
            if(tick==12001) requested_rate=133;
            if(tick==16001) requested_rate=20;
            if(tick==3001) requested_bits=8;
            if(tick==9001) requested_bits=3;
            if(tick==14001) requested_epoch=1;
            upstream_overflow=tick==4000;
            @(posedge clk);
            observed_reset=!filter_resetn;
            if(!filter_resetn) begin
                if(valid || config_valid) $fatal(1,"Transfer during filter reset");
                reset_width=reset_width+1;
            end else begin
                if(was_reset) begin
                    if(reset_width<32) $fatal(1,"Reset too short for async FIFO");
                    completed_resets=completed_resets+1;reset_width=0;
                end
                if(valid) begin
                    if(!ready) $fatal(1,"Sample admitted during backpressure");
                    total=total+1;
                end
                if(config_valid) begin
                    if(valid || !config_ready) $fatal(1,"Invalid configuration handshake");
                    if(active_bits!=requested_bits || config_rate!=requested_rate)
                        $fatal(1,"Committed superseded settings");
                    commits=commits+1;
                end
            end
            if(!filter_resetn) expected_gap=0;
            else if(dut.state==4 && !ready) expected_gap=1;
            #1;
            if(sample_gap!==expected_gap) $fatal(1,"Lost or stale sample gap flag");
            if(tick>4000 && tick<6001 && !overflow) $fatal(1,"Lost sticky overflow");
            if(was_reset && !filter_resetn && overflow) $fatal(1,"Overflow leaked across epoch");
            was_reset=observed_reset;
        end
        if(total<10000 || commits!=7 || completed_resets!=7)
            $fatal(1,"Lost samples or reset/configuration epoch");
        @(negedge clk);requested_run=0;
        repeat(40) @(posedge clk);
        if(filter_resetn || valid || overflow || sample_gap) $fatal(1,"Stop failed to clear history");
        $display("Single-stream control checks passed: %0d samples, %0d reset/configuration epochs",total,commits);
        $finish;
    end
endmodule
