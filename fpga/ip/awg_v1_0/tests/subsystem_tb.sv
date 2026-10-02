`timescale 1ns / 1ps
`ifndef CHANNEL_TEST_COUNT
`define CHANNEL_TEST_COUNT 1
`endif
`ifndef SINE_TEST_ENABLE
`define SINE_TEST_ENABLE 1
`endif
`ifndef PROFILE_PHASE_WIDTH
`define PROFILE_PHASE_WIDTH 48
`endif
`ifndef PROFILE_OUTPUT_WIDTH
`define PROFILE_OUTPUT_WIDTH 16
`endif
module subsystem_tb;
    localparam integer P = `PROFILE_PHASE_WIDTH;
    localparam integer O = `PROFILE_OUTPUT_WIDTH;
    localparam integer PEAK = (1 << (O-1))-2;
    localparam integer DIAGONAL = $rtoi(PEAK * 0.7071067811865476);
    localparam [63:0] QUARTER = 64'd1 << (P-2);
    localparam integer CHANNELS = `CHANNEL_TEST_COUNT;
    reg passed=0, aclk=0, clk=0, resetn=0, pause=0;
    always #3.5 aclk=~aclk;
    always #2 if (!pause) clk=~clk;
    reg [12:0] awaddr=0, araddr=0;
    reg awvalid=0,wvalid=0,arvalid=0,bready=0,rready=0;
    reg [31:0] wdata=0;
    reg [3:0] wstrb=0;
    wire awready,wready,arready,bvalid,rvalid;
    wire [1:0] bresp,rresp;
    wire [31:0] rdata;
    wire signed [O-1:0] dac0,dac1;
    // Instantiate the catalog package, including its embedded XCI files.
    pm_subsystem dut (
        .s_axi_aclk(aclk),.s_axi_aresetn(resetn),
        .s_axi_awaddr(awaddr),.s_axi_awprot(3'b0),.s_axi_awvalid(awvalid),.s_axi_awready(awready),
        .s_axi_wdata(wdata),.s_axi_wstrb(wstrb),.s_axi_wvalid(wvalid),.s_axi_wready(wready),
        .s_axi_bresp(bresp),.s_axi_bvalid(bvalid),.s_axi_bready(bready),
        .s_axi_araddr(araddr),.s_axi_arprot(3'b0),.s_axi_arvalid(arvalid),.s_axi_arready(arready),
        .s_axi_rdata(rdata),.s_axi_rresp(rresp),.s_axi_rvalid(rvalid),.s_axi_rready(rready),
        .sample_clk(clk),.dac0_data(dac0)
`ifdef TWO_CHANNELS
        ,.dac1_data(dac1)
`endif
    );
`ifndef TWO_CHANNELS
    assign dac1=0;
`endif
    task automatic write(input [12:0] address, input [31:0] data,
                         input [3:0] strobes=15, input integer skew=0,
                         input [1:0] response=0);
        reg [1:0] saved;
        begin
            fork
                begin
                    if (skew<0) repeat(-skew) @(negedge aclk);
                    @(negedge aclk); awaddr=address; awvalid=1;
                    do @(posedge aclk); while (!awready);
                    @(negedge aclk); awvalid=0;
                end
                begin
                    if (skew>0) repeat(skew) @(negedge aclk);
                    @(negedge aclk); wdata=data; wstrb=strobes; wvalid=1;
                    do @(posedge aclk); while (!wready);
                    @(negedge aclk); wvalid=0;
                end
            join
            wait(bvalid); saved=bresp;
            repeat(3) begin
                @(negedge aclk);
                if (!bvalid || bresp!==saved) $fatal(1,"Unstable AXI B response");
            end
            if (saved!==response) $fatal(1,"Write %h: response %d expected %d",address,saved,response);
            bready=1; @(negedge aclk); bready=0;
        end
    endtask

    task automatic read(input [12:0] address, output [31:0] data, input [1:0] response=0);
        reg [31:0] saved;
        begin
            @(negedge aclk); araddr=address; arvalid=1;
            do @(posedge aclk); while(!arready);
            @(negedge aclk); arvalid=0;
            wait(rvalid); saved=rdata;
            repeat(3) begin
                @(negedge aclk);
                if (!rvalid || rdata!==saved) $fatal(1,"Unstable AXI R response");
            end
            if (rresp!==response) $fatal(1,"Read response mismatch");
            data=saved; rready=1; @(negedge aclk); rready=0;
        end
    endtask

    task automatic word(input [12:0] address, input [63:0] data);
        write(address,data[31:0],15,2);
        write(address+4,data[63:32],15,-2);
    endtask

    task automatic commit(input [12:0] bank);
        reg [31:0] status;
        begin
            write(bank+'h10,7);
            do read(bank+'h0c,status); while(status[0]);
            repeat(150) @(negedge clk);
        end
    endtask
    task automatic check_outputs(input integer expected0,input integer expected1);
        begin
            repeat(16) begin
                @(negedge clk);
                if(dac0 < expected0-30 || dac0 > expected0+30)
                    $fatal(1,"Channel 0 got %d expected %d",dac0,expected0);
                if(CHANNELS==2 && (dac1 < expected1-30 || dac1 > expected1+30))
                    $fatal(1,"Channel 1 got %d expected %d",dac1,expected1);
            end
        end
    endtask
    reg [31:0] data;
    initial begin
        repeat(5) @(negedge aclk); resetn=1;
        read(0,data); if(data!==32'h504d0001) $fatal(1,"Wrong channel 0 ID");
        read(8,data);if(data[7:0]!==P) $fatal(1,"Wrong phase profile");
        read('h1c,data); if(data!==CHANNELS) $fatal(1,"Wrong channel count");
        write('h20,32'h12345678,15,4);
        write('h20,32'hffffffff,2,-4);
        read('h20,data); if(data!==32'h1234ff78) $fatal(1,"WSTRB routing failure");
        write('h21,0,15,0,2); read('h14,data,2);
        if(CHANNELS==1) begin
            read('h1000,data,2); write('h1020,77,15,-3,2);
            read('h20,data); if(data!==32'h1234ff78) $fatal(1,"Absent bank aliases channel 0");
        end else begin
            read('h1000,data); if(data!==32'h504d0001) $fatal(1,"Wrong channel 1 ID");
            read('h101c,data); if(data!==2) $fatal(1,"Wrong channel 1 count");
            write('h1020,77,15,-4);
            read('h20,data); if(data!==32'h1234ff78) $fatal(1,"Channel registers alias");
            read('h1020,data); if(data!==77) $fatal(1,"Channel 1 write lost");
            word('h1020,0); word('h1028,3*QUARTER);
            write('h1054,1); commit('h1000);
        end
        // Channel 0 positive peak, channel 1 negative peak; independent mute.
        word('h20,0); word('h28,QUARTER);
        write('h54,1); commit(0); check_outputs(PEAK,-PEAK);
        // Internal sine LUT: constant +1 PM shifts base=0 by 45 degrees.
        // A no-sine build exercises the exact square source through the same IP.
        word('h28,0); word('h30,0); word('h38,QUARTER);
        word('h40,QUARTER/2);
        write('h54,`SINE_TEST_ENABLE ? 3 : 'h103);
        if(!`SINE_TEST_ENABLE) word('h38,0);
        commit(0); check_outputs(DIAGONAL,-PEAK);
        write('h54,0); commit(0); check_outputs(0,-PEAK);
        // Read channel 0 while a channel 1 write response is backpressured.
        if(CHANNELS==2) begin
            @(negedge aclk); awaddr='h1050;wdata=123;wstrb=15;awvalid=1;wvalid=1;
            do @(posedge aclk); while(!awready||!wready);
            @(negedge aclk);awvalid=0;wvalid=0;
            wait(bvalid);
            read('h50,data);if(data!==1 || !bvalid || bresp!==0) $fatal(1,"Concurrent read corrupts B");
            @(negedge aclk);bready=1;@(negedge aclk);bready=0;
            read('h1050,data);if(data!==123) $fatal(1,"Concurrent write lost");
            // A paused sample clock holds each bank's commit independently.
            @(negedge clk);pause=1;
            write('h10,1);write('h1010,1);
            read('h0c,data);if(data!==1) $fatal(1,"Channel 0 not busy");
            read('h100c,data);if(data!==1) $fatal(1,"Channel 1 not busy");
            write('h1010,1,15,0,2);
            pause=0;repeat(150) @(negedge clk);
            read('h0c,data);if(data!==0) $fatal(1,"Channel 0 busy stuck");
            read('h100c,data);if(data!==0) $fatal(1,"Channel 1 busy stuck");
        end
        // Reset discards both configurations and mutes both outputs.
        @(negedge aclk);resetn=0;repeat(5) @(negedge aclk);resetn=1;
        repeat(150) @(negedge clk);check_outputs(0,0);
        passed=1;
        $display("PASS: packaged %0d-channel subsystem, sine=%0d, AXI routing, independent outputs, CDC and reset",CHANNELS,`SINE_TEST_ENABLE);
        $finish;
    end
    initial begin #2000000;$fatal(1,"Subsystem simulation timeout");end
endmodule
