`timescale 1 ns / 1 ps
// Exercise the appended production register offsets through actual AXI cores.
module test_p_axi_tb;
    reg clk=0,resetn=0;
    always #2 clk=~clk;
    reg [15:0] awaddr=0,araddr=0;
    reg [31:0] wdata=0;
    reg awvalid=0,wvalid=0,bready=0,arvalid=0,rready=0;
    wire awready,wready,bvalid,arready,rvalid;
    wire [31:0] rdata;
    wire [991:0] ctl;
    wire [863:0] sts;
    wire [31:0] status[0:1],snapshot[0:1];
    reg signed [15:0] i_in[0:1],q_in[0:1];
    axi_ctl_register #(.CTL_DATA_WIDTH(992),.PREDECODE_WRITES(1)) control(
        .aclk(clk),.aresetn(resetn),.ctl_data(ctl),
        .s_axi_awaddr(awaddr),.s_axi_awvalid(awvalid),.s_axi_awready(awready),
        .s_axi_wdata(wdata),.s_axi_wstrb(4'hf),.s_axi_wvalid(wvalid),.s_axi_wready(wready),
        .s_axi_bresp(),.s_axi_bvalid(bvalid),.s_axi_bready(bready),
        .s_axi_araddr(16'b0),.s_axi_arvalid(1'b0),.s_axi_arready(),
        .s_axi_rdata(),.s_axi_rresp(),.s_axi_rvalid(),.s_axi_rready(1'b0));
    assign sts={snapshot[1],snapshot[0],status[1],status[0],736'b0};
    axi_sts_register #(.STS_DATA_WIDTH(864)) readback(
        .aclk(clk),.aresetn(resetn),.sts_data(sts),
        .s_axi_awaddr(16'b0),.s_axi_awvalid(1'b0),.s_axi_awready(),
        .s_axi_wdata(32'b0),.s_axi_wvalid(1'b0),.s_axi_wready(),
        .s_axi_bresp(),.s_axi_bvalid(),.s_axi_bready(1'b0),
        .s_axi_araddr(araddr),.s_axi_arvalid(arvalid),.s_axi_arready(arready),
        .s_axi_rdata(rdata),.s_axi_rresp(),.s_axi_rvalid(rvalid),.s_axi_rready(rready));
    genvar ch;
    generate for(ch=0;ch<2;ch=ch+1) begin : loop_dut
        manual_p_corrector dut(.clk(clk),.resetn(resetn),.freq_in(17'sd0),.phase_in(32'sd0),
            .i_in(i_in[ch]),.q_in(q_in[ch]),.cx(ctl[864+64*ch +: 32]),.cy(ctl[896+64*ch +: 32]),
            .request_fast(ctl[832+ch]),.snapshot_request(ctl[840+ch]),
            .enabled(ctl[513+32*ch +: 3]),.active_banks(4'b0),.table_command(9'b0),.table_data(64'b0),
            .fast_corr(),.slow_corr(),.snapshot(snapshot[ch]),.path_status(status[ch]));
    end endgenerate
    integer checked=0,k;
    reg [31:0] value;
    task write_reg(input [15:0] offset,input [31:0] word);
        reg a_done,w_done;
        begin
            @(negedge clk);awaddr=offset;wdata=word;awvalid=1;wvalid=1;
            while(awvalid || wvalid) begin
                @(posedge clk);a_done=awvalid && awready;w_done=wvalid && wready;
                @(negedge clk);if(a_done)awvalid=0;if(w_done)wvalid=0;
            end
            bready=1;@(posedge clk);while(!bvalid) @(posedge clk);
            @(negedge clk);bready=0;checked=checked+1;
        end
    endtask
    task read_reg(input [15:0] offset,output [31:0] word);
        begin
            @(negedge clk);araddr=offset;arvalid=1;
            @(posedge clk);while(!arready) @(posedge clk);
            @(negedge clk);arvalid=0;rready=1;
            @(posedge clk);while(!rvalid) @(posedge clk);
            word=rdata;@(negedge clk);rready=0;checked=checked+1;
        end
    endtask
    task expect_status(input integer channel,input [2:0] mask,input [2:0] expected);
        integer watchdog;
        begin
            watchdog=0;value=~expected;
            while((value&mask)!=expected) begin
                read_reg(92+4*channel,value);watchdog=watchdog+1;
                if(watchdog>30) $fatal(1,"P AXI acknowledgement timeout channel=%0d",channel);
            end
        end
    endtask
    initial begin
        i_in[0]=4096;q_in[0]=0;i_in[1]=0;q_in[1]=-2048;
        repeat(6) @(negedge clk);resetn=1;
        write_reg(64,14);write_reg(68,14);
        write_reg(104,32'h300); // Independent capture toggles, both loops.
        expect_status(0,4,4);expect_status(1,4,4);
        read_reg(100,value);if(value!==32'h00001000) $fatal(1,"Loop 0 snapshot mapping failed");
        read_reg(104,value);if(value!==32'hf8000000) $fatal(1,"Loop 1 snapshot mapping failed");
        write_reg(108,0);write_reg(112,166886);
        write_reg(116,333772);write_reg(120,0);
        write_reg(104,32'h302); // Fast channel 1 only.
        expect_status(1,3,3);expect_status(0,1,0);
        write_reg(104,32'h303);expect_status(0,3,3);
        // Held capture requests must not repeat while input/data words change.
        i_in[0]=8192;q_in[0]=1000;
        repeat(80) @(negedge clk);
        read_reg(100,value);if(value!==32'h00001000) $fatal(1,"Held AXI capture repeated");
        write_reg(104,32'h302);expect_status(0,1,0);expect_status(1,1,1);
        write_reg(104,32'h202); // Recapture channel 0; preserve channel 1 mode.
        expect_status(0,4,0);expect_status(1,1,1);
        read_reg(100,value);if(value!==32'h03e82000) $fatal(1,"Recapture data mismatch");
        write_reg(68,0);expect_status(1,1,0);
        @(negedge clk);resetn=0;
        repeat(5) @(negedge clk);
        if(ctl || status[0][0] || status[1][0] || snapshot[0] || snapshot[1])
            $fatal(1,"P AXI peripheral reset failed");
        $display("P AXI checks passed: %0d transactions; both modes, coherent captures, channel isolation and reset",checked);
        $finish;
    end
    initial begin #20000; $fatal(1,"P AXI timeout"); end
endmodule
