`timescale 1ns/1ps
module test_paired_cic_control;
    reg clk=0; always #2.5 clk=~clk;
    reg resetn=0,ready_x=0,ready_y=0,config_ready_x=0,config_ready_y=0;
    reg [15:0] requested_rate=0;
    wire valid,config_valid,filter_resetn;
    wire [15:0] config_rate;
    paired_cic_control dut(clk,resetn,requested_rate,ready_x,ready_y,valid,
                           config_ready_x,config_ready_y,config_valid,config_rate,filter_resetn);
    integer tick,count_x=0,count_y=0,total=0,old_x=0,old_y=0;
    integer commits=0,reset_width=0,completed_resets=0,rate=20;
    integer last_x=0,last_y=0,outputs_x=0,outputs_y=0;
    reg was_reset=1;
    initial begin
        for(tick=0;tick<20000;tick=tick+1) begin
            @(negedge clk);
            ready_x=(tick%113)<83;
            ready_y=(tick%137)<91;
            config_ready_x=(tick%11)<7;
            config_ready_y=(tick%13)<5;
            if(tick==8) resetn=1;
            if(tick==10) requested_rate=20;
            if(tick==6001) requested_rate=67;
            if(tick==6007) requested_rate=100; // Supersede a rate during reset.
            if(tick==12001) requested_rate=133;
            if(tick==16001) requested_rate=20;
            @(posedge clk);
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
                    if(config_rate!=requested_rate) $fatal(1,"Committed superseded rate");
                    rate=config_rate;commits=commits+1;
                end
                if(count_x!=count_y || last_x!=last_y) $fatal(1,"Different accepted ADC sample identities");
                if(outputs_x!=outputs_y) $fatal(1,"Different decimated output timestamps");
            end
            // Negative control: former always-valid inputs accept unequal clocks.
            if(ready_x) old_x=old_x+1;
            if(ready_y) old_y=old_y+1;
            was_reset=!filter_resetn;
        end
        if(total<9000) $fatal(1,"Insufficient progress under backpressure");
        if(commits!=4 || completed_resets!=4) $fatal(1,"Lost configuration or reset epoch");
        if(old_x==old_y) $fatal(1,"Negative control did not reproduce independent loss");
        $display("Paired CIC control checks passed: %0d matched samples, %0d reset/configuration epochs; old counts %0d/%0d",total,commits,old_x,old_y);
        $finish;
    end
endmodule
