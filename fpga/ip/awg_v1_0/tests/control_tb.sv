`timescale 1ns / 1ps
`ifndef PHASE_TEST_WIDTH
`define PHASE_TEST_WIDTH 32
`endif
module control_tb;
    localparam integer P = `PHASE_TEST_WIDTH;
    localparam integer B=(P+7)/8;
    reg passed=0, aclk=0, clk=0, resetn=0, pause=0;
    always #3.5 aclk=~aclk;
    always #2 if(!pause) clk=~clk;
    reg [11:0] awaddr=0, araddr=0;
    reg awvalid=0, wvalid=0, bready=0, arvalid=0, rready=0;
    reg [31:0] wdata=0;
    wire awready,wready,bvalid,arready,rvalid;
    wire [1:0] bresp,rresp;
    wire [31:0] rdata;
    wire [2*B*8+7:0] phase;
    wire valid;
    awg #(.PHASE_WIDTH(P), .MOD_WIDTH(16), .ENABLE_SINE(0),
        .ENABLE_SQUARE(1), .ENABLE_PULSE(0), .ENABLE_TRIANGLE(0),
        .ENABLE_UP_RAMP(0), .ENABLE_DOWN_RAMP(0), .ENABLE_UNIFORM(0),
        .ENABLE_GAUSSIAN(0), .ENABLE_PRBS(0), .ENABLE_BPSK(1)) dut (
        .s_axi_aclk(aclk),.s_axi_aresetn(resetn),.s_axi_awaddr(awaddr),.s_axi_awprot(3'b0),
        .s_axi_awvalid(awvalid),.s_axi_awready(awready),.s_axi_wdata(wdata),.s_axi_wstrb(4'hf),
        .s_axi_wvalid(wvalid),.s_axi_wready(wready),.s_axi_bresp(bresp),.s_axi_bvalid(bvalid),.s_axi_bready(bready),
        .s_axi_araddr(araddr),.s_axi_arprot(3'b0),.s_axi_arvalid(arvalid),.s_axi_arready(arready),
        .s_axi_rdata(rdata),.s_axi_rresp(rresp),.s_axi_rvalid(rvalid),.s_axi_rready(rready),
        .sample_clk(clk),.m_axis_phase_tdata(phase),.m_axis_phase_tvalid(valid),
        .s_axis_mod_data_tdata(16'b0),.s_axis_mod_data_tuser({(3*P+26){1'b0}}),.s_axis_mod_data_tvalid(1'b0),
        .s_axis_carrier_tdata(16'b0),.s_axis_carrier_tuser(1'b0),.s_axis_carrier_tvalid(1'b0)
    );
    task automatic write(input [11:0] address,input [31:0] value,input [1:0] response=0);
        begin
            @(negedge aclk); awaddr=address; wdata=value; awvalid=1; wvalid=1;
            @(posedge aclk); if(!awready||!wready) $fatal(1,"Not ready");
            @(negedge aclk); awvalid=0;wvalid=0;
            wait(bvalid); if(bresp!==response) $fatal(1,"Wrong B response %d",bresp);
            @(negedge aclk); bready=1; @(negedge aclk); bready=0;
        end
    endtask
    task automatic read(input [11:0] address,output [31:0] value);
        begin
            @(negedge aclk); araddr=address;arvalid=1;
            @(posedge aclk); if(!arready) $fatal(1,"Read not ready");
            @(negedge aclk); arvalid=0;
            wait(rvalid); if(rresp!==0) $fatal(1,"Wrong read response");value=rdata;
            @(negedge aclk);rready=1;@(negedge aclk);rready=0;
        end
    endtask
    integer restart_count=0;
    reg [P-1:0] expected;
    reg checking=0;
    always @(posedge clk) if(valid && phase[2*B*8]) begin
        restart_count=restart_count+1;
        if(checking && phase[B*8 +: P]!==expected) $fatal(1,"Incoherent mailbox got %h expected %h",phase[B*8 +: P],expected);
    end
    reg [31:0] data;
    initial begin
        repeat(5) @(negedge aclk);resetn=1;
        repeat(10) @(negedge clk);
        read(4,data);if(data!==514) $fatal(1,"Wrong reduced capabilities");
        read(8,data);if(data[7:0]!==P) $fatal(1,"Wrong phase width");
        write('h54,3); write('h10,1,2); // Disabled sine rejected.
        write('h54,'h103);write('h40,17);write('h28,7);
        // Freeze the receiving clock to keep the first commit pending.
        @(negedge clk);pause=1;
        write('h10,7);
        read('h0c,data);if(data!==1) $fatal(1,"Busy not set");
        write('h28,99); // Shadow edits must not modify the pending payload.
        write('h10,1,2); // Second commit must fail without replacing payload.
        expected=24;checking=1;pause=0;
        wait(restart_count==1);
        read('h0c,data);if(data!==0) $fatal(1,"Busy did not clear");
        read('h28,data);if(data!==99) $fatal(1,"Shadow not preserved");
        expected=116;write('h10,7);wait(restart_count==2);
        // Same protocol at non-byte-aligned widths, and reserved padding zero.
        if(phase[B*8 +: P]!==116) $fatal(1,"Wrong phase packing");
        if(B*8>P && ((phase >> (B*8+P)) & ((1 << (B*8-P))-1))!==0) $fatal(1,"Nonzero padding");
        checking=0;
        passed=1;$display("PASS: reduced IP, %0d-bit packing, stopped clock and coherent commit",P);$finish;
    end
    initial begin #100000; $fatal(1,"Control test timeout");end
endmodule
