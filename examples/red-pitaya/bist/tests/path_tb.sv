`timescale 1ns/1ps
// DNA is a vendor primitive; datapath verification uses a deterministic stub.
module DNA_PORT(input CLK,DIN,READ,SHIFT,output DOUT);
    reg [56:0] data=57'h123456789abcdef;
    assign DOUT=data[0];
    always @(posedge CLK) if(READ) data<=57'h123456789abcdef; else if(SHIFT) data<={DIN,data[56:1]};
endmodule
module path_tb;
    reg clk=0; always #4 clk=~clk;
    reg rst=0,valid=0,last=0,ready=0;
    reg [31:0] mode=0,capture=0,samples=0,snapshot=0,reset_diag=0,seed=32'hb157,data=0;
    wire inready,outvalid,outlast,resetn;
    wire [31:0] outdata,bd;
    wire [31:0] ba;
    wire [3:0] we;
    wire [63:0] cycles,tx,rx,dna;
    wire [31:0] overflow,packets,discontinuities,done,snapshot_id;
    reg [31:0] memory[0:1023]; reg [31:0] q=0;
    reg [13:0] adc0=0,adc1=0;
    wire [13:0] dac0,dac1;
    bist_path dut(.aclk(clk),.aresetn(rst),.dna_clk(clk),.mode(mode),.capture(capture),.samples(samples),.snapshot(snapshot),
        .reset_diag(reset_diag),.seed(seed),.s_axis_tdata(data),.s_axis_tkeep(4'hf),.s_axis_tvalid(valid),.s_axis_tlast(last),
        .s_axis_tready(inready),.m_axis_tdata(outdata),.m_axis_tkeep(),.m_axis_tvalid(outvalid),.m_axis_tlast(outlast),
        .m_axis_tready(ready),.adc0(adc0),.adc1(adc1),.dac0(dac0),.dac1(dac1),.bram_addr(ba),.bram_data(bd),.bram_we(we),.bram_q(q),
        .cycles_snapshot(cycles),.tx_snapshot(tx),.rx_snapshot(rx),.data_resetn(resetn),.dna(dna),
        .overflow_snapshot(overflow),.packets_snapshot(packets),.discontinuities_snapshot(discontinuities),.capture_done(done),.snapshot_id(snapshot_id));
    always @(posedge clk) begin
        if(we!=0 && ba[1:0]!=0) $fatal(1,"BRAM-controller byte address is unaligned");
        if(we!=0) memory[ba[11:2]]<=bd;
        q<=memory[ba[11:2]]; adc0<=adc0+1; adc1<=adc1+3;
    end
    task send(input [31:0] value,input bit tail);
        @(negedge clk); data=value;valid=1;last=tail;
        do @(posedge clk); while(!inready);
        @(negedge clk); valid=0;last=0;
    endtask
    task snap;
        @(negedge clk);snapshot=snapshot+1;
        repeat(3) @(negedge clk);
        if(snapshot_id!=snapshot) $fatal(1,"Snapshot handshake failed");
    endtask
    integer received=0;
    reg [31:0] held;
    initial begin
        repeat(4) @(negedge clk);rst=1;
        fork
            begin for(integer i=0;i<256;i=i+1) send((32'(i)*32'h9e3779b9)^seed,i==255); end
            begin
                for(integer i=0;i<800;i=i+1) begin
                    @(negedge clk);ready=(i%5!=0);
                    @(posedge clk);
                    if(outvalid&&ready) begin
                        if(outdata!=((32'(received)*32'h9e3779b9)^seed)) $fatal(1,"Digital loopback corruption");
                        if(outlast!=(received==255)) $fatal(1,"TLAST mismatch");
                        received=received+1;
                    end
                end
            end
        join
        snap(); if(tx!=256||rx!=256||packets!=1||discontinuities!=0) $fatal(1,"Atomic counters mismatch");
        ready=1;send(32'hbad,1);snap();if(discontinuities!=1) $fatal(1,"Discontinuity not counted");
        @(negedge clk);mode=256;repeat(3)@(negedge clk);mode=1;
        for(integer i=0;i<1024;i=i+1)send(32'(i)^32'h12345678,i==1023);
        for(integer i=0;i<1024;i=i+1)if(memory[i]!=(32'(i)^32'h12345678)) $fatal(1,"Upload address/data mismatch");
        @(negedge clk);mode=2;samples=32;capture=1;received=0;
        while(received<32) begin
            @(posedge clk);if(outvalid&&ready) begin
                if(outlast!=(received==31))$fatal(1,"Capture TLAST mismatch");
                received=received+1;
            end
        end
        repeat(3)@(negedge clk);if(!done)$fatal(1,"Capture done missing");
        snap();if(overflow!=0)$fatal(1,"Unexpected capture overflow");
        @(negedge clk);ready=0;capture=2;
        wait(outvalid);held=outdata;repeat(30)begin @(posedge clk);if(outdata!=held)$fatal(1,"AXIS payload changed under backpressure");end
        snap();if(overflow==0)$fatal(1,"Capture loss not diagnosed");
        $display("Datapath simulation PASS");$finish;
    end
    initial begin #1000000;$fatal(1,"Simulation deadline exceeded");end
endmodule
