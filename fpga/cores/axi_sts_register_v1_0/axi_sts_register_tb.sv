`timescale 1ns/1ps
module axi_sts_register_tb #(
    parameter integer WORDS=11,
    parameter integer DATA_WIDTH=32,
    parameter integer ADDR_WIDTH=16
);
    localparam integer BYTES=DATA_WIDTH/8;
    localparam integer LATENCY=2;
    reg clk=0, resetn=0;
    always #2 clk=~clk;
    reg [WORDS*DATA_WIDTH-1:0] status;
    reg [ADDR_WIDTH-1:0] araddr=0, awaddr=0;
    reg arvalid=0, rready=0, awvalid=0, wvalid=0, bready=0;
    reg [DATA_WIDTH-1:0] wdata=0;
    wire arready, rvalid, awready, wready, bvalid;
    wire [DATA_WIDTH-1:0] rdata;
    wire [1:0] rresp, bresp;
    axi_sts_register #(.STS_DATA_WIDTH(WORDS*DATA_WIDTH),
        .AXI_DATA_WIDTH(DATA_WIDTH), .AXI_ADDR_WIDTH(ADDR_WIDTH)) dut(
        .aclk(clk), .aresetn(resetn), .sts_data(status),
        .s_axi_araddr(araddr), .s_axi_arvalid(arvalid), .s_axi_arready(arready),
        .s_axi_rdata(rdata), .s_axi_rresp(rresp), .s_axi_rvalid(rvalid), .s_axi_rready(rready),
        .s_axi_awaddr(awaddr), .s_axi_awvalid(awvalid), .s_axi_awready(awready),
        .s_axi_wdata(wdata), .s_axi_wvalid(wvalid), .s_axi_wready(wready),
        .s_axi_bresp(bresp), .s_axi_bvalid(bvalid), .s_axi_bready(bready));

    integer epoch=0, index, byte_index;
    // Changing every byte each clock catches wrong selection, sampling time,
    // and response mutation during a stall.
    always @(negedge clk) begin
        epoch=epoch+1;
        for(index=0;index<WORDS;index=index+1)
            for(byte_index=0;byte_index<BYTES;byte_index=byte_index+1)
                status[index*DATA_WIDTH+byte_index*8 +: 8]=index*37+epoch*11+byte_index*7;
    end
    integer cycle=0, accepted_cycle=0, model_index=0;
    integer reads=0, completed=0, cancelled=0, aws=0, ws=0, writes=0;
    reg pending=0;
    reg [DATA_WIDTH-1:0] expected=0;
    always @(posedge clk) begin
        cycle=cycle+1;
        if(!resetn) begin
            if(pending) cancelled=cancelled+1;
            pending=0; aws=0; ws=0;
        end else begin
            if(arvalid && arready) begin
                if(pending) $fatal(1,"Accepted a second outstanding read");
                pending=1; accepted_cycle=cycle; model_index=araddr/BYTES; reads=reads+1;
            end
            if(pending && cycle==accepted_cycle+2) begin
                expected=0;
                if(model_index<WORDS) expected=status[model_index*DATA_WIDTH +: DATA_WIDTH];
            end
            if(rvalid) begin
                if(!pending || cycle<accepted_cycle+LATENCY+1)
                    $fatal(1,"Unexpected or premature read response");
                if(rdata!==expected || rresp!==0)
                    $fatal(1,"Read mismatch address=%0d got=%h expected=%h",model_index,rdata,expected);
                if(rready) begin pending=0; completed=completed+1; end
            end
            if(awvalid && awready) aws=aws+1;
            if(wvalid && wready) ws=ws+1;
            if(bvalid) begin
                if(aws<1 || ws<1 || bresp!==0) $fatal(1,"Unexpected write response");
                if(bready) begin aws=aws-1; ws=ws-1; writes=writes+1; end
            end
        end
    end
    // Check after register updates. RVALID must not depend on RREADY.
    always @(negedge clk) begin
        if(resetn && pending && cycle>=accepted_cycle+LATENCY) begin
            if(!rvalid || rdata!==expected || rresp!==0)
                $fatal(1,"Missing or unstable held response at cycle %0d",cycle);
            if(arready) $fatal(1,"ARREADY asserted while response outstanding");
        end
    end
    task send_address(input [ADDR_WIDTH-1:0] address);
        begin
            @(negedge clk); araddr=address; arvalid=1;
            @(posedge clk); while(!arready) @(posedge clk);
            @(negedge clk); arvalid=0;
        end
    endtask
    task drain_read(input integer stall);
        begin
            repeat(stall) @(negedge clk);
            @(negedge clk); rready=1;
            @(posedge clk); while(!rvalid) @(posedge clk);
            @(negedge clk); rready=0;
        end
    endtask
    task read_word(input [ADDR_WIDTH-1:0] address,input integer stall);
        begin send_address(address); drain_read(stall); end
    endtask
    task reset_bus;
        begin
            @(negedge clk); resetn=0; arvalid=0; rready=0; awvalid=0; wvalid=0; bready=0;
            repeat(3) @(negedge clk);
            if(rvalid || bvalid) $fatal(1,"Response survived reset");
            resetn=1;
        end
    endtask
    task send_aw;
        begin
            @(negedge clk); awaddr={ADDR_WIDTH{1'b1}}; awvalid=1;
            @(posedge clk); while(!awready) @(posedge clk);
            @(negedge clk); awvalid=0;
        end
    endtask
    task send_w;
        begin
            @(negedge clk); wdata={DATA_WIDTH{1'b1}}; wvalid=1;
            @(posedge clk); while(!wready) @(posedge clk);
            @(negedge clk); wvalid=0;
        end
    endtask
    task drain_write(input integer stall);
        begin
            repeat(stall) @(negedge clk);
            @(negedge clk); bready=1;
            @(posedge clk); while(!bvalid) @(posedge clk);
            @(negedge clk); bready=0;
        end
    endtask
    integer word_number, offset, delay_cycles, n;
    integer seed=32'h6278c09a;
    reg [ADDR_WIDTH-1:0] address;
    initial begin
        reset_bus;
        for(word_number=0;word_number<WORDS;word_number=word_number+1)
            for(offset=0;offset<BYTES;offset=offset+1)
                read_word(word_number*BYTES+offset,(word_number+offset)%9);
        read_word(WORDS*BYTES,12);
        // A high address with a zero low index must not alias status word zero.
        read_word(32768,12);
        read_word({ADDR_WIDTH{1'b1}},12);
        for(n=0;n<120;n=n+1) begin
            address=$unsigned($random(seed))%((WORDS+7)*BYTES);
            read_word(address,$unsigned($random(seed))%12);
        end
        // Hold the next request throughout an outstanding, stalled reply.
        send_address(0);
        @(negedge clk); araddr=(WORDS-1)*BYTES; arvalid=1;
        drain_read(LATENCY+4);
        @(posedge clk); while(!arready) @(posedge clk);
        @(negedge clk); arvalid=0;
        drain_read(8);
        // Cancel at every stage and while the completed reply is stalled.
        for(delay_cycles=0;delay_cycles<=LATENCY+2;delay_cycles=delay_cycles+1) begin
            send_address((WORDS-1)*BYTES);
            repeat(delay_cycles) @(negedge clk);
            reset_bus;
            read_word(0,8);
        end
        send_aw; repeat(4) @(negedge clk); send_w; drain_write(8);
        send_w; repeat(4) @(negedge clk); send_aw; drain_write(8);
        fork send_aw; send_w; join
        // Queue another write behind the first stalled B response.
        fork send_aw; send_w; join
        drain_write(5); drain_write(5);
        fork
            begin send_aw; send_w; drain_write(8); end
            begin read_word((WORDS-1)*BYTES,8); read_word(WORDS*BYTES,2); end
        join
        fork send_aw; send_w; join
        reset_bus;
        read_word(0,8);
        repeat(4) @(negedge clk);
        if(pending || aws || ws || reads!=completed+cancelled || writes!=5)
            $fatal(1,"Transaction mismatch reads=%0d completed=%0d cancelled=%0d writes=%0d",reads,completed,cancelled,writes);
        $display("PASS: AXI status WORDS=%0d DATA=%0d ADDR=%0d latency=%0d reads=%0d cancelled=%0d writes=%0d",
                 WORDS,DATA_WIDTH,ADDR_WIDTH,LATENCY,completed,cancelled,writes);
        $finish;
    end
    initial begin #1000000; $fatal(1,"AXI status timeout"); end
endmodule
