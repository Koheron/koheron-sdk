`timescale 1ns/1ps
module DNA_PORT(input CLK,DIN,READ,SHIFT,output DOUT);
    assign DOUT=1'b1;
endmodule
module path_tb;
    reg clk=0;always #4 clk=~clk;
    reg rst=0,valid=0,last=0,ready=0;
    reg [31:0] mode=0,capture=0,samples=0,snapshot=0,reset_diag=0,seed=32'hf011b157;
    reg [31:0] stalls=0,fault=0,packet_bytes=0,decimation=8,irq_period=0,irq_ack=0,data=0;
    reg [3:0] keep=15;
    wire inready,outvalid,outlast,resetn,irq;
    wire [3:0] outkeep,we;
    wire [31:0] outdata,ba,bd,overflow,packets,discontinuities,done,snapshot_id;
    wire [31:0] checker_errors,keep_errors,input_stalls,output_stalls,irq_count,irq_pending;
    wire [63:0] cycles,tx,rx,dna;
    wire [13:0] dac0,dac1;
    full_path dut(.aclk(clk),.aresetn(rst),.dna_clk(clk),.mode(mode),.capture(capture),.samples(samples),.snapshot(snapshot),
        .reset_diag(reset_diag),.seed(seed),.stalls(stalls),.fault(fault),.packet_bytes(packet_bytes),.decimation(decimation),.irq_period(irq_period),.irq_ack(irq_ack),
        .s_axis_tdata(data),.s_axis_tkeep(keep),.s_axis_tvalid(valid),.s_axis_tlast(last),.s_axis_tready(inready),
        .m_axis_tdata(outdata),.m_axis_tkeep(outkeep),.m_axis_tvalid(outvalid),.m_axis_tlast(outlast),.m_axis_tready(ready),
        .adc0(14'h123),.adc1(14'h321),.dac0(dac0),.dac1(dac1),.bram_addr(ba),.bram_data(bd),.bram_we(we),.bram_q(32'h12343210),
        .cycles_snapshot(cycles),.tx_snapshot(tx),.rx_snapshot(rx),.data_resetn(resetn),.dna(dna),
        .overflow_snapshot(overflow),.packets_snapshot(packets),.discontinuities_snapshot(discontinuities),.capture_done(done),.snapshot_id(snapshot_id),
        .checker_errors_snapshot(checker_errors),.keep_errors_snapshot(keep_errors),.input_stalls_snapshot(input_stalls),.output_stalls_snapshot(output_stalls),
        .irq_count_snapshot(irq_count),.irq_pending(irq_pending),.irq(irq));
    reg stalled_before=0;
    reg [36:0] held;
    always @(posedge clk) begin
        if(resetn && stalled_before && (!outvalid || {outlast,outkeep,outdata}!==held))
            $fatal(1,"AXIS payload changed while VALID was stalled");
        stalled_before<=resetn&&outvalid&&!ready;
        held<={outlast,outkeep,outdata};
    end
    task reset_path;
        @(negedge clk);mode=256;valid=0;ready=0;stalls=0;fault=0;irq_period=0;
        repeat(3)@(negedge clk);mode=0;repeat(3)@(negedge clk);
    endtask
    task snap;
        @(negedge clk);snapshot=snapshot+1;repeat(3)@(negedge clk);
        if(snapshot_id!==snapshot)$fatal(1,"snapshot ack mismatch");
    endtask
    task send(input [31:0] word,input [3:0] lanes,input bit tail);
        @(negedge clk);data=word;keep=lanes;last=tail;valid=1;
        do @(posedge clk);while(!inready);
        @(negedge clk);valid=0;
    endtask
    integer received,iterations;
    initial begin
        #2000000;$fatal(1,"simulation deadline");
    end
    initial begin
        repeat(4)@(negedge clk);rst=1;
        reset_path();packet_bytes=13;stalls=16'hf080;received=0;
        fork
            begin for(integer i=0;i<4;++i)send((32'(i)*32'h9e3779b9)^seed,i==3?1:15,i==3);end
            begin
                for(integer i=0;i<2000;++i)begin
                    @(negedge clk);ready=(i%7!=0);@(posedge clk);
                    if(outvalid&&ready)begin
                        if(outdata!==((32'(received)*32'h9e3779b9)^seed))$fatal(1,"loopback payload");
                        if(outkeep!==(received==3?4'h1:4'hf)||outlast!==(received==3))$fatal(1,"KEEP/LAST");
                        received=received+1;
                    end
                end
            end
        join
        snap();if(received!=4||tx!=4||rx!=4||packets!=1||checker_errors||keep_errors||discontinuities||input_stalls==0)$fatal(1,"loopback counters");
        for(integer n=1;n<=17;++n)begin
            reset_path();mode=3;packet_bytes=n;capture=capture+1;received=0;iterations=0;
            while(received<(n+3)/4&&iterations<200)begin
                @(negedge clk);ready=(iterations%5!=0);@(posedge clk);
                if(outvalid&&ready)begin
                    if(outdata!==((32'(received)*32'h9e3779b9)^seed))$fatal(1,"generator data");
                    if(outkeep!==((n-received*4)>=4?4'hf:4'((1<<(n-received*4))-1)))$fatal(1,"generator tail KEEP");
                    if(outlast!==(received==(n-1)/4))$fatal(1,"generator LAST");received=received+1;
                end
                iterations=iterations+1;
            end
            if(received!=(n+3)/4)$fatal(1,"generator did not complete");
        end
        reset_path();packet_bytes=16;ready=1;
        for(integer i=0;i<4;++i)send(((32'(i)*32'h9e3779b9)^seed)^(i==2?1:0),15,i==3);
        repeat(3)@(negedge clk);snap();if(checker_errors!=1)$fatal(1,"checker failed corruption control");
        reset_path();packet_bytes=2;ready=1;send(seed,5,1);snap();if(keep_errors!=1)$fatal(1,"bad KEEP not detected");
        reset_path();mode=3;packet_bytes=64;fault=2;capture=capture+1;ready=1;
        repeat(30)@(negedge clk);snap();if(rx!=4||packets!=1)$fatal(1,"early TLAST injection");
        reset_path();mode=3;packet_bytes=64;fault=4;capture=capture+1;ready=1;
        repeat(40)@(negedge clk);snap();if(rx!=16||packets!=0)$fatal(1,"missing TLAST injection");
        reset_path();mode=2;samples=32;decimation=1;capture=capture+1;ready=0;
        repeat(80)@(negedge clk);snap();if(overflow==0)$fatal(1,"ADC overflow invisible");
        reset_path();if(outvalid)$fatal(1,"reset did not flush stalled beat");
        irq_period=8;repeat(20)@(negedge clk);if(!irq||irq_pending!=1)$fatal(1,"IRQ not latched");
        irq_period=0;irq_ack=irq_ack+1;repeat(3)@(negedge clk);if(irq)$fatal(1,"IRQ did not clear");
        snapshot=32'hfffffffe;snap();snap();if(snapshot_id!=0)$fatal(1,"snapshot uint32 wrap");
        $display("PASS full datapath: elastic AXIS, partial KEEP, generator/checker, faults, ADC loss, IRQ and wrap");$finish;
    end
endmodule
