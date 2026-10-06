`timescale 1ns/1ps
module test_monitor_stream_tb;
    reg fast_clk=0, slow_clk=0;
    always #2 fast_clk=~fast_clk;
    // FCLK1 is 1000/7 MHz, nominally described as 143 MHz.
    always #3.5 slow_clk=~slow_clk;
    reg resetn=0, run=0, sink_ready=1;
    reg [15:0] rate=4;
    wire filter_resetn, slow_resetn, data_valid, data_ready, config_valid, config_ready;
    wire [15:0] config_rate;
    wire [3:0] bits;
    wire gap, overflow;
    phase_stream_control #(.RATE_STEP(2)) control(
        .aclk(fast_clk), .aresetn(resetn), .requested_rate(rate),
        .requested_bits(4'd0), .requested_epoch(1'b0), .requested_run(run),
        .active_bits(bits), .data_ready(data_ready), .data_valid(data_valid),
        .config_ready(1'b1), .config_valid(config_valid), .config_rate(config_rate),
        .upstream_overflow(1'b0), .overflow(overflow),
        .filter_resetn(filter_resetn), .sample_gap(gap));
    wire [15:0] slow_rate;
    phase_stream_cdc cdc(.clk(slow_clk), .status_clk(fast_clk), .resetn_in(filter_resetn),
        .rate_in(config_rate), .rate(slow_rate),
        .metadata_in({gap, overflow, bits}), .packet_in(32'd0), .resetn(slow_resetn),
        .metadata(), .packet_status());
    reg [31:0] phase=0;
    always @(posedge fast_clk) phase <= phase+1;
    wire [39:0] fixed_data, crossed_data, cic_data, filtered_data;
    wire fixed_valid, fixed_ready, crossed_valid, crossed_ready, cic_valid, cic_ready, filtered_valid;
    phase_fixed_decimator fixed_stage(fast_clk,filter_resetn,phase,data_valid,data_ready,
                                     fixed_data,fixed_valid,fixed_ready);
    system_phase_cic_clock_converter_0 bridge(
        .s_axis_aclk(fast_clk), .s_axis_aresetn(filter_resetn),
        .s_axis_tdata(fixed_data), .s_axis_tvalid(fixed_valid), .s_axis_tready(fixed_ready),
        .m_axis_aclk(slow_clk), .m_axis_aresetn(slow_resetn),
        .m_axis_tdata(crossed_data), .m_axis_tvalid(crossed_valid), .m_axis_tready(crossed_ready));
    phase_cic_decimator cic(slow_clk,slow_resetn,slow_rate,crossed_data,crossed_valid,crossed_ready,
                           cic_data,cic_valid,cic_ready);
    system_fir_0 fir(.aclk(slow_clk), .aresetn(slow_resetn),
        .s_axis_data_tdata(cic_data), .s_axis_data_tvalid(cic_valid),
        .s_axis_data_tready(cic_ready), .m_axis_data_tdata(filtered_data),
        .m_axis_data_tvalid(filtered_valid), .m_axis_data_tready(sink_ready));

    reg [39:0] expected[0:262143];
    integer produced=0, consumed=0, outputs=0;
    always @(posedge fast_clk) begin
        if (!filter_resetn) produced=0;
        else if (fixed_valid && fixed_ready) begin
            if (produced >= 262144) $fatal(1,"Test queue exhausted");
            expected[produced]=fixed_data;
            produced=produced+1;
        end
    end
    always @(posedge slow_clk) begin
        if (!slow_resetn) begin consumed=0; outputs=0; end
        else begin
            if (crossed_valid && crossed_ready) begin
                if (consumed >= produced || crossed_data !== expected[consumed])
                    $fatal(1,"CIC/FIR clock crossing dropped, reordered or tore sample %0d",consumed);
                consumed=consumed+1;
            end
            if (filtered_valid && sink_ready) begin
                if ((^filtered_data) === 1'bx) $fatal(1,"Unknown FIR output");
                outputs=outputs+1;
            end
        end
    end
    task check_rate(input integer requested, input integer count);
        integer start_count, observed;
        begin
            @(negedge fast_clk); run=0; rate=requested; sink_ready=1;
            repeat(100) @(negedge fast_clk);
            run=1;
            wait(data_valid);
            wait(filtered_valid);
            repeat(64) @(negedge fast_clk);
            if (gap) $fatal(1,"Startup gap at R=%0d",requested);
            start_count=outputs;
            repeat(2*requested*count) @(negedge fast_clk);
            observed=outputs-start_count;
            if (gap || observed < count-2 || observed > count+2)
                $fatal(1,"Throughput R=%0d: %0d expected %0d gap=%0b",requested,observed,count,gap);
            $display("PASS: 250/143 MHz stream R=%0d, %0d FIR samples, no input gaps",requested,observed);
        end
    endtask
    initial begin
        repeat(20) @(negedge fast_clk);
        resetn=1;
        check_rate(4,1024);
        // Long backpressure must be detected, then cleared by a fresh epoch.
        sink_ready=0;
        repeat(4000) @(negedge slow_clk);
        if (!gap) $fatal(1,"Backpressure did not report a sample gap");
        check_rate(20,128);
        check_rate(8192,8);
        check_rate(4,1024);
        $display("PASS: production CIC, asynchronous crossing and 143 MHz FIR throughput/reset recovery");
        $finish;
    end
    initial begin #30000000; $fatal(1,"Monitor stream timeout"); end
endmodule
