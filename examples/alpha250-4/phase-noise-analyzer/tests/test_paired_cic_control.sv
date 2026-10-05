`timescale 1ns/1ps
module test_paired_cic_control;
    reg clk=0; always #2.5 clk=~clk;
    reg resetn=0,ready_x=0,ready_y=0,config_ready_x=0,config_ready_y=0;
    reg [15:0] requested_rate=0;
    wire valid,config_valid,filter_resetn;
    wire [15:0] config_rate;
    reg [3:0] requested_bits=0;
    reg requested_run=0;
    reg expected_gap=0;
    wire sample_gap;
    reg requested_epoch=0, upstream_overflow_x=0, upstream_overflow_y=0;
    wire [3:0] active_bits;
    wire overflow_x, overflow_y;
    paired_cic_control dut(.aclk(clk),.aresetn(resetn),.*,
        .data_ready_x(ready_x),.data_ready_y(ready_y),.data_valid(valid));
    integer tick,count_x=0,count_y=0,total=0,old_x=0,old_y=0;
    integer commits=0,reset_width=0,completed_resets=0,rate=20;
    integer last_x=0,last_y=0,outputs_x=0,outputs_y=0;
    reg was_reset=1, observed_reset=1;
    initial begin
        for(tick=0;tick<20000;tick=tick+1) begin
            @(negedge clk);
            ready_x=(tick%113)<83;
            ready_y=(tick%137)<91;
            config_ready_x=(tick%11)<7;
            config_ready_y=(tick%13)<5;
            if(tick==8) resetn=1;
            if(tick==10) requested_rate=20;
            if(tick==50) requested_run=1;
            if(tick==6001) requested_rate=67;
            if(tick==6007) requested_rate=100; // Supersede a rate during reset.
            if(tick==12001) requested_rate=133;
            if(tick==16001) requested_rate=20;
            if(tick==3001) requested_bits=8;
            if(tick==9001) requested_bits=3;
            if(tick==14001) requested_epoch=1;
            upstream_overflow_x=tick==4000;
            upstream_overflow_y=tick==10000;
            @(posedge clk);
            observed_reset=!filter_resetn;
            if(!filter_resetn) begin
                if(valid || config_valid) $fatal(1,"Transfer while filter reset is asserted");
                count_x=0;count_y=0;outputs_x=0;outputs_y=0;
                reset_width=reset_width+1;
            end else begin
                if(was_reset) begin
                    if(reset_width<32) $fatal(1,"Reset too short for async FIFO");
                    completed_resets=completed_resets+1;reset_width=0;
                end
                if(valid && ready_x) begin
                    count_x=count_x+1;last_x=tick;total=total+1;
                    if(count_x%rate==0) outputs_x=outputs_x+1;
                end
                if(valid && ready_y) begin
                    count_y=count_y+1;last_y=tick;
                    if(count_y%rate==0) outputs_y=outputs_y+1;
                end
                if(config_valid) begin
                    if(valid) $fatal(1,"Sample admitted before rate configuration completed");
                    if(!config_ready_x || !config_ready_y) $fatal(1,"Unilateral rate update");
                    if(active_bits!=requested_bits) $fatal(1,"Committed superseded precision");
                    if(config_rate!=requested_rate) $fatal(1,"Committed superseded rate");
                    rate=config_rate;commits=commits+1;
                end
                if(count_x!=count_y || last_x!=last_y) $fatal(1,"Different accepted ADC sample identities");
                if(outputs_x!=outputs_y) $fatal(1,"Different decimated output timestamps");
            end
            if (!filter_resetn) expected_gap=0;
            else if(dut.state==4 && (!ready_x || !ready_y)) expected_gap=1;
            #1;
            if(sample_gap!==expected_gap) $fatal(1,"Lost or stale sample gap flag");
            if(tick>4000 && tick<6001 && !overflow_x) $fatal(1,"Lost sticky X overflow");
            if(tick>10000 && tick<12001 && !overflow_y) $fatal(1,"Lost sticky Y overflow");
            if(was_reset && !filter_resetn && (overflow_x || overflow_y)) $fatal(1,"Overflow leaked across epoch");
            // Negative control: former always-valid inputs accept unequal clocks.
            if(ready_x) old_x=old_x+1;
            if(ready_y) old_y=old_y+1;
            was_reset=observed_reset;
        end
        if(total<9000) $fatal(1,"Insufficient progress under backpressure");
        if(commits!=7 || completed_resets!=7) $fatal(1,"Lost configuration or reset epoch: %0d commits, %0d resets",commits,completed_resets);
        if(old_x==old_y) $fatal(1,"Negative control did not reproduce independent loss");
        $display("Paired CIC control checks passed: %0d matched samples, %0d reset/configuration epochs; old counts %0d/%0d",total,commits,old_x,old_y);
        $finish;
    end
endmodule
