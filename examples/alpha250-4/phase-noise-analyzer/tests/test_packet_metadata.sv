`timescale 1ns/1ps
module test_packet_metadata;
    reg aclk=0; always #5 aclk=~aclk;
    reg aresetn=0;
    reg [31:0] s_axis_0_tdata=0,s_axis_1_tdata=0;
    reg [5:0] s_axis_0_tuser=0,s_axis_1_tuser=0;
    reg s_axis_0_tvalid=0,s_axis_1_tvalid=0,m_axis_tready=0;
    wire s_axis_0_tready,s_axis_1_tready,m_axis_tvalid,m_axis_tlast;
    wire [31:0] m_axis_tdata;
    wire [3:0] m_axis_tkeep;
    reg [12:0] s_axi_awaddr=0,s_axi_araddr=0;
    reg [3:0] s_axi_wstrb=15;
    reg [31:0] s_axi_wdata=0;
    reg s_axi_awvalid=0,s_axi_wvalid=0,s_axi_bready=1,s_axi_arvalid=0,s_axi_rready=1;
    wire s_axi_awready,s_axi_wready,s_axi_bvalid,s_axi_arready,s_axi_rvalid;
    wire [31:0] s_axi_rdata;
    wire [1:0] s_axi_bresp,s_axi_rresp;
    axi_stream_packet_mux dut(.*);
    integer accepted=0,cycles=0;
    reg expected_sel=0, continuous_testing=0;
    reg stalled=0;
    reg [32:0] held=0;
    always @(posedge aclk) if(aresetn) begin
        cycles=cycles+1;
        if(cycles>100000) $fatal(1,"Packet did not complete");
        if(stalled && (!m_axis_tvalid || {m_axis_tlast,m_axis_tdata} !== held))
            $fatal(1,"Payload changed during stall");
        stalled=m_axis_tvalid && !m_axis_tready;
        held={m_axis_tlast,m_axis_tdata};
        if(m_axis_tvalid && m_axis_tready) begin
            if(continuous_testing) expected_sel=(accepted/16)%2;
            if(m_axis_tdata !== accepted+ (expected_sel ? 1000 : 0)) $fatal(1,"Wrong sample order");
            if(m_axis_tlast !== (accepted%16==15)) $fatal(1,"Wrong packet length");
            if(s_axis_0_tready && s_axis_1_tready) $fatal(1,"Both inputs advanced");
            if(m_axis_tkeep!=15) $fatal(1,"Partial beat");
            accepted=accepted+1;
        end
    end
    // Model queued FIFO words; metadata changes only on accepted input beats.
    always @(negedge aclk) begin
        m_axis_tready=cycles%7!=0 && cycles%37<29;
        s_axis_0_tdata=accepted;
        s_axis_1_tdata=1000+accepted;
        s_axis_0_tuser={(accepted%16==3), (accepted%16==7), accepted%16<8 ? 4'd2 : 4'd5};
        s_axis_1_tuser={(accepted%16==15),(accepted%16==15),4'd8};
    end
    task write_csr(input [31:0] data);
        begin
            @(negedge aclk); s_axi_wdata=data;s_axi_awvalid=1;s_axi_wvalid=1;
            do @(posedge aclk); while(!(s_axi_awready && s_axi_wready));
            @(negedge aclk);s_axi_awvalid=0;s_axi_wvalid=0;
            wait(s_axi_bvalid); @(negedge aclk);
        end
    endtask
    task check_status(input [31:0] expected, input [12:0] address=4);
        begin
            @(negedge aclk);s_axi_araddr=address;s_axi_arvalid=1;
            do @(posedge aclk); while(!s_axi_arready);
            @(negedge aclk);s_axi_arvalid=0;
            wait(s_axi_rvalid);
            if(s_axi_rdata!==expected) $fatal(1,"Wrong FIFO packet metadata: %h != %h",s_axi_rdata,expected);
            @(negedge aclk);
        end
    endtask
    initial begin
        repeat(4) @(negedge aclk);aresetn=1;
        s_axis_0_tvalid=1;s_axis_1_tvalid=1;
        write_csr((16<<2)|2);
        wait(accepted==16);repeat(3) @(negedge aclk);
        check_status((1<<8)|32'h72); // precision 2, overflow and mixed 2->5
        expected_sel=1;
        write_csr((16<<2)|3);
        wait(accepted==32);repeat(3) @(negedge aclk);
        check_status((2<<8)|32'h58); // precision 8, overflow only on final beat
        repeat(20) @(negedge aclk);
        if(accepted!=32) $fatal(1,"One-shot kept draining after TLAST");
        check_status('h72, 'h1000);
        check_status('h58, 'h1004);
        write_csr(16<<2); // Stop clears progress before a fresh SG ring.
        check_status(0);
        continuous_testing=1;
        write_csr((1<<16)|(16<<2)|2);
        wait(accepted>=32+1026*16);
        @(negedge aclk);s_axis_0_tvalid=0;s_axis_1_tvalid=0;
        repeat(3) @(negedge aclk);
        // Check metadata ring wrap and hardware packet alternation. Old X
        // metadata remains independent of the current Y precision/gap flags.
        check_status('h72, 'h1000);
        check_status('h58, 'h1004);
        write_csr(16<<2);
        check_status(0);
        $display("FIFO packet metadata checks passed: queued precision changes, X/Y selection, overflow/gaps, continuous alternation, ring wrap, stalls and CSR publication");
        $finish;
    end
endmodule
