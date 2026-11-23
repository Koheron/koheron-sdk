`timescale 1ns/1ps
module ddr_tb;
    reg clk=0;always #4 clk=~clk;
    reg rst=0;
    reg [31:0] command=0,config_word=0,address=32'h18000000,byte_count=0,seed=32'h12345678,burst_beats=16,pauses=0,abort_request=0;
    wire [31:0] done,busy,errors,mismatches,first_address,bursts;
    wire [63:0] first_expected,first_actual,transferred,cycles;
    wire [31:0] awaddr,araddr;wire [7:0] awlen,arlen,wstrb;
    wire [2:0] awsize,arsize,awprot,arprot;wire [1:0] awburst,arburst;
    wire awlock,arlock;wire [3:0] awcache,arcache,awqos,arqos;
    wire awvalid,arvalid,wvalid,wlast,bready,rready;
    wire [63:0] wdata;
    reg awready=0,arready=0,wready=0,bvalid=0,rvalid=0,rlast=0;
    reg [1:0] bresp=0,rresp=0;reg [63:0] rdata=0;
    ddr_tester dut(.aclk(clk),.aresetn(rst),.*,
        .m_axi_awaddr(awaddr),.m_axi_awlen(awlen),.m_axi_awsize(awsize),.m_axi_awburst(awburst),.m_axi_awlock(awlock),.m_axi_awcache(awcache),.m_axi_awprot(awprot),.m_axi_awqos(awqos),.m_axi_awvalid(awvalid),.m_axi_awready(awready),
        .m_axi_wdata(wdata),.m_axi_wstrb(wstrb),.m_axi_wlast(wlast),.m_axi_wvalid(wvalid),.m_axi_wready(wready),.m_axi_bresp(bresp),.m_axi_bvalid(bvalid),.m_axi_bready(bready),
        .m_axi_araddr(araddr),.m_axi_arlen(arlen),.m_axi_arsize(arsize),.m_axi_arburst(arburst),.m_axi_arlock(arlock),.m_axi_arcache(arcache),.m_axi_arprot(arprot),.m_axi_arqos(arqos),.m_axi_arvalid(arvalid),.m_axi_arready(arready),
        .m_axi_rdata(rdata),.m_axi_rresp(rresp),.m_axi_rlast(rlast),.m_axi_rvalid(rvalid),.m_axi_rready(rready));
    reg [63:0] memory[0:16383];
    integer tick=0,wleft=0,rleft=0,wi=0,ri=0,issued=0;
    reg inject_b=0,inject_r=0,inject_last=0;
    reg held_aw=0,held_ar=0,held_w=0;
    reg [39:0] saved_aw,saved_ar;reg [72:0] saved_w;
    always @(posedge clk)begin
        if(rst)begin
            tick<=tick+1;
            if(held_aw&&(!awvalid||{awaddr,awlen}!==saved_aw))$fatal(1,"AW changed under stall");
            if(held_ar&&(!arvalid||{araddr,arlen}!==saved_ar))$fatal(1,"AR changed under stall");
            if(held_w&&(!wvalid||{wdata,wstrb,wlast}!==saved_w))$fatal(1,"W changed under stall");
            held_aw<=awvalid&&!awready;saved_aw<={awaddr,awlen};
            held_ar<=arvalid&&!arready;saved_ar<={araddr,arlen};
            held_w<=wvalid&&!wready;saved_w<={wdata,wstrb,wlast};
            awready<=tick%7==0&&wleft==0&&!bvalid;
            arready<=tick%5==0&&rleft==0&&!rvalid;
            wready<=tick%3!=0;
            if(awvalid&&awready)begin
                if((awaddr&4095)+(int'(awlen)+1)*8>4096||awsize!=3||awburst!=1)$fatal(1,"illegal AW burst");
                if(awaddr<32'h18000000||awaddr>=32'h20000000)$fatal(1,"address fence escaped");
                wleft<=int'(awlen)+1;wi<=int'((awaddr&32'h1ffff)>>3);issued<=issued+1;
            end
            if(wvalid&&wready)begin
                if(wleft<=0||wlast!=(wleft==1))$fatal(1,"write framing");
                for(integer j=0;j<8;++j)if(wstrb[j])memory[wi][j*8+:8]<=wdata[j*8+:8];
                wi<=wi+1;wleft<=wleft-1;
                if(wlast)begin bvalid<=1;bresp<=inject_b?2:0;end
            end
            if(bvalid&&bready)bvalid<=0;
            if(arvalid&&arready)begin
                if((araddr&4095)+(int'(arlen)+1)*8>4096||arsize!=3||arburst!=1)$fatal(1,"illegal AR burst");
                rleft<=int'(arlen)+1;ri<=int'((araddr&32'h1ffff)>>3);issued<=issued+1;
            end
            if(rvalid&&rready)begin rvalid<=0;rleft<=rleft-1;ri<=ri+1;end
            if(!rvalid&&rleft>0&&tick%4!=0)begin
                rvalid<=1;rdata<=memory[ri];rresp<=inject_r?2:0;rlast<=rleft==1||inject_last;
                if(inject_last)rleft<=1;
            end
        end
    end
    task run(input [31:0] cfg,input [31:0] addr,input [31:0] n,input [31:0] burst);
        @(negedge clk);config_word=cfg;address=addr;byte_count=n;burst_beats=burst;command=command+1;
        do @(negedge clk);while(done!=command);
        if(busy)$fatal(1,"completion must be quiescent");
    endtask
    integer before_issued;
    initial begin #50000000;$fatal(1,"DDR simulation deadline");end
    initial begin
        for(integer i=0;i<16384;++i)memory[i]=0;
        repeat(5)@(negedge clk);rst=1;
        for(integer kind=0;kind<4;++kind)begin
            pauses=32'h8080;
            run(32'hff01|(kind<<4),32'h18000ff8,8192,256);
            if(errors||transferred!=8192||bursts!=5)$fatal(1,"write or 4K split count");
            run(32'hff02|(kind<<4),32'h18000ff8,8192,16);
            if(errors||mismatches||transferred!=8192)$fatal(1,"pattern check");
        end
        pauses=0;
        // Byte lanes, and an independent expected address-pattern calculation.
        seed=0;run(32'hff11,32'h18000000,512,1);
        seed=32'h12345678;run(32'h5501,32'h18000000,512,16);
        for(integer i=0;i<64;++i)begin
            reg [31:0] a;reg [63:0] p;a=32'h18000000+32'(i)*8;
            p={~a^seed,a^{seed[15:0],seed[31:16]}};
            for(integer j=0;j<8;++j)if(memory[i][8*j+:8]!==((j%2==0)?p[8*j+:8]:8'b0))$fatal(1,"WSTRB lane isolation");
        end
        run(32'haa01,32'h18000000,512,64);run(32'hff02,32'h18000000,512,256);
        if(mismatches||errors)$fatal(1,"merged strobes");
        memory[17]=memory[17]^64'h100000000;
        run(32'hff02,32'h18000000,512,16);
        if(mismatches!=1||first_address!=32'h18000088||first_actual==first_expected)$fatal(1,"first failure diagnostics");
        before_issued=issued;
        run(32'hff01,32'h17fffff8,16,16);if(errors!=1)$fatal(1,"lower fence");
        run(32'hff01,32'h1ffffff8,16,16);if(errors!=1)$fatal(1,"upper fence");
        run(32'hff01,32'h18000000,0,16);if(errors!=1)$fatal(1,"zero length");
        run(32'hff01,32'h18000001,8,16);if(errors!=1)$fatal(1,"alignment");
        run(32'hff01,32'h18000000,8,257);if(errors!=1)$fatal(1,"oversized burst");
        if(issued!=before_issued||transferred!=0)$fatal(1,"invalid operation issued AXI request");
        inject_b=1;run(32'hff01,32'h18000000,512,16);if(errors!=1)$fatal(1,"BRESP detector");inject_b=0;
        inject_r=1;run(32'hff02,32'h18000000,512,16);if(errors!=16)$fatal(1,"RRESP detector");inject_r=0;
        inject_last=1;run(32'hff02,32'h18000000,512,16);if(errors!=1)$fatal(1,"RLAST detector");inject_last=0;
        fork
            run(32'hff01,32'h18000000,65536,256);
            begin wait(wvalid&&!wready);@(negedge clk);abort_request=1;end
        join
        if(transferred==0||transferred>=65536||errors)$fatal(1,"abort did not drain current burst");
        abort_request=0;
        run(32'hff01,32'h18000000,512,16);run(32'hff02,32'h18000000,512,16);
        if(errors||mismatches)$fatal(1,"post-abort recovery");
        $display("PASS DDR master: random channel stalls, 4KiB split, burst lengths, strobes, independent pattern, corruption, fences, BRESP/RRESP/RLAST and draining abort");$finish;
    end
endmodule
