`timescale 1ns/1ps
module uart_tb;
    reg clk=0; always #4 clk=~clk;
    reg rst=0;
    reg [11:0] awaddr=0,araddr=0;
    reg awvalid=0,wvalid=0,arvalid=0;
    reg [31:0] wdata=0;
    wire awready,wready,bvalid,arready,rvalid,tx,irq;
    wire [31:0] rdata;
    bist_uart dut(.aclk(clk),.aresetn(rst),.s_axi_awaddr(awaddr),.s_axi_awvalid(awvalid),.s_axi_awready(awready),
        .s_axi_wdata(wdata),.s_axi_wstrb(4'hf),.s_axi_wvalid(wvalid),.s_axi_wready(wready),
        .s_axi_bresp(),.s_axi_bvalid(bvalid),.s_axi_bready(1'b1),.s_axi_araddr(araddr),.s_axi_arvalid(arvalid),
        .s_axi_arready(arready),.s_axi_rdata(rdata),.s_axi_rresp(),.s_axi_rvalid(rvalid),.s_axi_rready(1'b1),
        .rx(tx),.tx(tx),.irq(irq));
    task write(input [11:0] addr,input [31:0] value,input integer skew);
        @(negedge clk); awaddr=addr; awvalid=1;
        do @(posedge clk); while (!awready);
        @(negedge clk); awvalid=0;
        repeat(skew) @(negedge clk);
        wdata=value; wvalid=1;
        do @(posedge clk); while (!wready);
        @(negedge clk); wvalid=0;
        wait(bvalid); @(negedge clk);
    endtask
    task read(input [11:0] addr,output [31:0] value);
        @(negedge clk); araddr=addr; arvalid=1;
        do @(posedge clk); while (!arready);
        @(negedge clk); arvalid=0;
        wait(rvalid); value=rdata; @(negedge clk);
    endtask
    reg [31:0] value;
    initial begin
        repeat(5) @(negedge clk); rst=1;
        write(8,1,3);
        for(integer i=0;i<64;i=i+1) write(0,i*13+7,i%4);
        repeat(200000) @(negedge clk);
        if(!irq) $fatal(1,"RX level IRQ missing");
        for(integer i=0;i<64;i=i+1) begin
            read(0,value); if(value!=8'((i*13+7))) $fatal(1,"FIFO payload mismatch %0d %0d",i,value);
        end
        repeat(3) @(negedge clk); if(irq) $fatal(1,"IRQ did not clear after RX service");
        read(24,value); if(value!=0) $fatal(1,"UART framing/FIFO errors %0d",value);
        read(28,value); if(value<2712 || value>2713) $fatal(1,"Baud frame timing %0d",value);
        write(8,2,1); if(!irq) $fatal(1,"TX demand IRQ missing");
        write(8,0,1); if(irq) $fatal(1,"TX IRQ did not mask");
        write(12,1,2); read(16,value); if(value!=0) $fatal(1,"Reset did not clear counters");
        $display("UART simulation PASS"); $finish;
    end
    initial begin #3000000; $fatal(1,"Simulation deadline exceeded"); end
endmodule
