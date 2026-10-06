`timescale 1ns/1ps
module test_split_cic_tb;
    reg fast_clk=0, slow_clk=0;
    always #2 fast_clk=~fast_clk;
    always #3.5 slow_clk=~slow_clk;
    reg resetn=0, input_valid=0;
    reg [15:0] total_rate=4;
    reg ready=1;
    wire input_ready;
    integer index=0, received=0;
    reg [31:0] phase=0;
    always @* begin
        phase=(32'(index)*32'h9e3779b9) ^ (32'(index)<<7) ^ (32'(index)>>3);
        if (index>=1024) phase=index<8*total_rate ? 32'h7fffffff : 32'h80000000;
    end
    wire [39:0] fixed_data, crossed_data, output_data;
    wire fixed_valid, fixed_ready, crossed_valid, crossed_ready, output_valid;
    phase_fixed_decimator fixed_stage(fast_clk,resetn,phase,input_valid,input_ready,
                                     fixed_data,fixed_valid,fixed_ready);
    system_phase_cic_clock_converter_0 bridge(
        .s_axis_aclk(fast_clk), .s_axis_aresetn(resetn),
        .s_axis_tdata(fixed_data), .s_axis_tvalid(fixed_valid), .s_axis_tready(fixed_ready),
        .m_axis_aclk(slow_clk), .m_axis_aresetn(resetn),
        .m_axis_tdata(crossed_data), .m_axis_tvalid(crossed_valid), .m_axis_tready(crossed_ready));
    phase_cic_decimator programmable_stage(slow_clk,resetn,total_rate,crossed_data,
        crossed_valid,crossed_ready,output_data,output_valid,ready);
    reg [39:0] expected [0:15];
    string vector_dir;
    reg [39:0] held_data;
    reg held_valid=0;
    always @(posedge fast_clk) begin
        if (!resetn) index<=0;
        else if (input_valid && input_ready) index<=index+1;
    end
    always @(posedge slow_clk) begin
        if (!resetn) begin received=0; held_valid=0; end
        else begin
            if (held_valid && (!output_valid || output_data !== held_data))
                $fatal(1,"Output changed under backpressure");
            held_valid=output_valid && !ready;
            held_data=output_data;
            if (output_valid && ready) begin
                if (received>=16 || output_data !== expected[received])
                    $fatal(1,"R=%0d sample=%0d actual=%h expected=%h",total_rate,received,output_data,expected[received]);
                received=received+1;
            end
        end
    end
    task check_rate(input integer r);
        begin
            @(negedge fast_clk); resetn=0; input_valid=0; total_rate=r; ready=1;
            $readmemh($sformatf("%s/reference_%0d.mem",vector_dir,r),expected);
            repeat(100) @(negedge fast_clk);
            resetn=1;
            repeat(50) @(negedge fast_clk);
            input_valid=1;
            fork
                begin
                    wait(index==r*16);
                    @(negedge fast_clk); input_valid=0;
                end
                begin
                    repeat(20) @(negedge slow_clk); ready=0;
                    repeat(100) @(negedge slow_clk); ready=1;
                end
            join
            wait(received==16);
            repeat(20) @(negedge slow_clk);
            $display("PASS: split CIC R=%0d, exact 40-bit reference and backpressure",r);
        end
    endtask
    initial begin
        if (!$value$plusargs("vectors=%s",vector_dir)) $fatal(1,"Missing vector directory");
        check_rate(4); check_rate(6); check_rate(8); check_rate(10);
        check_rate(14); check_rate(20); check_rate(32); check_rate(126);
        check_rate(128); check_rate(130); check_rate(8190); check_rate(8192);
        $display("PASS: full-precision split CIC numerical equivalence, normalization and epoch recovery");
        $finish;
    end
    initial begin #3000000; $fatal(1,"Split CIC timeout"); end
endmodule
