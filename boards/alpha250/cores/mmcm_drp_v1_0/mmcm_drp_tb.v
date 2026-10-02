`timescale 1 ns / 1 ps
module mmcm_drp_tb;
    reg clk=0; always #2.5 clk=~clk;
    reg [31:0] ctl=0;
    wire [31:0] sts;
    wire reset,den,dwe;
    wire [6:0] addr;
    wire [15:0] di;
    reg ready=0,locked=0;
    reg [15:0] data=0;
    integer pulses=0;
    mmcm_drp dut(clk,ctl,sts,reset,den,dwe,addr,di,ready,data,locked);
    always @(posedge clk) if(den) pulses=pulses+1;
    initial begin
        #20;
        if (!reset) $fatal(1,"MMCM must stay reset before software initialization");
        @(negedge clk) ctl=32'h81080000;
        @(posedge den); #1;
        if(!reset || dwe || addr!=8 || sts[24]) $fatal(1,"Read launched incorrectly");
        repeat(4) @(negedge clk);
        data=16'hBEEF;ready=1;
        @(negedge clk);ready=0;
        if(sts[24]!=1 || sts[15:0]!=16'hBEEF) $fatal(1,"Read completion missing");
        repeat(8) @(negedge clk);
        if(pulses!=1) $fatal(1,"Stable request retriggered");
        ctl=32'h808A0082;
        @(posedge den); #1;
        if(!dwe || addr!=10 || di!=16'h0082 || sts[24]!=1) $fatal(1,"Write launched incorrectly");
        repeat(3) @(negedge clk);
        ready=1;data=16'h0082;
        @(negedge clk);ready=0;
        if(sts[24]!=0 || sts[15:0]!=16'h0082) $fatal(1,"Write completion missing");
        ctl[31]=0;locked=1;
        repeat(5) @(negedge clk);
        if(reset || !sts[16] || pulses!=2) $fatal(1,"Reset/lock handling failed");
        $display("PASS: DRP transactions, delayed acknowledgments, reset and lock status");
        $finish;
    end
    initial begin #2000; $fatal(1,"Test timed out"); end
endmodule
