`timescale 1 ns / 1 ps
module test_table_system_tb;
    reg clk=0, resetn=0;
    always #2 clk=~clk;
    reg [15:0] awaddr=0, araddr=0;
    reg [31:0] wdata=0;
    reg awvalid=0, wvalid=0, bready=0, arvalid=0, rready=0;
    wire awready,wready,bvalid,arready,rvalid;
    wire [31:0] rdata;
    wire [767:0] control;
    wire [639:0] status;
    wire [31:0] ack;
    wire [7:0] banks;
    wire [511:0] coefficients;
    wire [8:0] command0,command1;
    wire [63:0] data;
    reg signed [16:0] freq[0:1];
    reg signed [31:0] phase[0:1];
    reg signed [63:0] gains[0:7];
    reg signed [63:0] requested_coefficient=0;
    reg [7:0] previous_banks=0;
    reg [31:0] request=0;
    reg commit_expected=0;
    integer target_expected=0,cycles=0,checked=0,transactions=0,writes=0,commits=0;
    integer seed=472032,loop_index,j,n,target,kind,address,factor;
    reg [31:0] value;
    reg bank;
    reg signed [63:0] coefficient;

    axi_ctl_register #(.CTL_DATA_WIDTH(768),.PREDECODE_WRITES(1)) ctl (
        .aclk(clk),.aresetn(resetn),.ctl_data(control),
        .s_axi_awaddr(awaddr),.s_axi_awvalid(awvalid),.s_axi_awready(awready),
        .s_axi_wdata(wdata),.s_axi_wstrb(4'hf),.s_axi_wvalid(wvalid),.s_axi_wready(wready),
        .s_axi_bresp(),.s_axi_bvalid(bvalid),.s_axi_bready(bready),
        .s_axi_araddr(16'b0),.s_axi_arvalid(1'b0),.s_axi_arready(),
        .s_axi_rdata(),.s_axi_rresp(),.s_axi_rvalid(),.s_axi_rready(1'b0)
    );
    assign status={coefficients,24'b0,banks,ack,64'b0};
    axi_sts_register #(.STS_DATA_WIDTH(640)) sts (
        .aclk(clk),.aresetn(resetn),.sts_data(status),
        .s_axi_awaddr(16'b0),.s_axi_awvalid(1'b0),.s_axi_awready(),
        .s_axi_wdata(32'b0),.s_axi_wvalid(1'b0),.s_axi_wready(),
        .s_axi_bresp(),.s_axi_bvalid(),.s_axi_bready(1'b0),
        .s_axi_araddr(araddr),.s_axi_arvalid(arvalid),.s_axi_arready(arready),
        .s_axi_rdata(rdata),.s_axi_rresp(),.s_axi_rvalid(rvalid),.s_axi_rready(rready)
    );
    gain_programmer programmer(clk,resetn,control[672 +: 32],control[704 +: 64],
                               ack,banks,coefficients,command0,command1,data);

    genvar channel;
    generate for(channel=0;channel<2;channel=channel+1) begin : loop_dut
        wire [2:0] enabled=control[513+32*channel +: 3];
        wire [15:0] fast,slow;
        table_corrector #(.FUSED(1),.GAIN_STAGES(2),.TAIL_GAIN_STAGES(3),.FINAL_CSA_LEVELS(2))
            dut(clk,freq[channel],phase[channel],enabled,banks[4*channel +: 4],
                channel ? command1 : command0,data,fast,slow);
        reg [31:0] rp[0:1],rpi[0:1],ri2[0:2];
        reg [63:0] ri3[0:2];
        reg [31:0] first_sum=0,acc2=0;
        reg signed [47:0] acc1=0;
        reg [63:0] acc3=0;
        reg signed [127:0] product_p,product_pi,product_i2,product_i3;
        integer x;
        initial begin
            for(x=0;x<2;x=x+1) begin rp[x]=0;rpi[x]=0;end
            for(x=0;x<3;x=x+1) begin ri2[x]=0;ri3[x]=0;end
        end
        always @(posedge clk) begin
            product_p=$signed(freq[channel])*gains[4*channel];
            product_pi=$signed(phase[channel])*gains[4*channel+1];
            product_i2=$signed(acc1)*gains[4*channel+2];
            product_i3=$signed(acc2)*gains[4*channel+3];
            rp[0]<=product_p[42:11];rpi[0]<=product_pi[58:27];
            ri2[0]<=product_i2[90:59];ri3[0]<=product_i3[74:11];
            rp[1]<=rp[0];rpi[1]<=rpi[0];
            for(x=1;x<3;x=x+1) begin ri2[x]<=ri2[x-1];ri3[x]<=ri3[x-1];end
            first_sum<=rp[1]+rpi[1];
            if(!enabled[0]) acc1<=0;else acc1<=acc1+$signed(first_sum);
            if(!enabled[1]) acc2<=0;else acc2<=acc2+first_sum+ri2[2];
            if(!enabled[2]) acc3<=0;else acc3<=acc3+ri3[2];
            #1;
            if(cycles>12 && {dut.p,dut.pi,dut.i2,dut.i3,dut.acc1,dut.acc2,dut.acc3,fast,slow} !==
                 {rp[1],rpi[1],ri2[2],ri3[2],acc1,acc2,acc3,acc2[31:16],~acc3[63],acc3[62:48]})
                $fatal(1,"Integrated controller mismatch channel=%0d cycle=%0d",channel,cycles);
        end
    end endgenerate

    always @(negedge clk) begin
        for(loop_index=0;loop_index<2;loop_index=loop_index+1) begin
            freq[loop_index]=$random(seed);phase[loop_index]=$random(seed);
        end
    end
    always @(posedge clk) begin
        cycles=cycles+1;
        if(command0[8] || command1[8]) writes=writes+1;
        #1;
        for(j=0;j<8;j=j+1) begin
            if(banks[j]!=previous_banks[j]) begin
                if(!commit_expected || target_expected!=j)
                    $fatal(1,"Unexpected bank change: target=%0d cycle=%0d",j,cycles);
                gains[j]=requested_coefficient;
                commit_expected=0;
                commits=commits+1;
            end
            if(coefficients[64*j +: 64] !== gains[j])
                $fatal(1,"Incorrect gain readback target=%0d cycle=%0d",j,cycles);
        end
        previous_banks=banks;
        if(cycles>12) checked=checked+1;
    end

    task axi_write(input [15:0] offset,input [31:0] word);
        reg aw_done,w_done;
        begin
            @(negedge clk);awaddr=offset;wdata=word;awvalid=1;wvalid=1;
            while(awvalid || wvalid) begin
                @(posedge clk);aw_done=awvalid && awready;w_done=wvalid && wready;
                @(negedge clk);if(aw_done)awvalid=0;if(w_done)wvalid=0;
            end
            repeat(transactions%3) @(negedge clk);
            bready=1;
            @(posedge clk);while(!bvalid) @(posedge clk);
            @(negedge clk);bready=0;
        end
    endtask
    task axi_read(input [15:0] offset,output [31:0] word);
        begin
            @(negedge clk);araddr=offset;arvalid=1;
            @(posedge clk);while(!arready) @(posedge clk);
            @(negedge clk);arvalid=0;rready=1;
            @(posedge clk);while(!rvalid) @(posedge clk);
            word=rdata;
            @(negedge clk);rready=0;
        end
    endtask
    task transact(input [9:0] flags,input signed [63:0] payload,input reject_expected);
        integer watchdog;
        reg [31:0] response;
        begin
            axi_write(88,payload[31:0]);
            // Leave the old command asserted while its data changes in two
            // separate bus writes. It must not repeat or corrupt either bank.
            repeat(3) @(negedge clk);
            axi_write(92,payload[63:32]);
            repeat(3) @(negedge clk);
            request={~request[31],21'b0,flags};
            if(flags[9] && !reject_expected) begin
                target_expected=flags[8:6];requested_coefficient=payload;commit_expected=1;
            end
            axi_write(84,request);
            response=~request;watchdog=0;
            while((response & 32'hbfffffff)!=request) begin
                axi_read(8,response);watchdog=watchdog+1;
                if(watchdog>20) $fatal(1,"Programming acknowledgement timeout");
            end
            if(response[30]!==reject_expected) $fatal(1,"Unexpected programming status %h",response);
            if(commit_expected) $fatal(1,"Acknowledged before atomic commit");
            repeat(transactions%7) @(negedge clk);
            transactions=transactions+1;
        end
    endtask
    initial begin
        for(n=0;n<2;n=n+1) begin freq[n]=0;phase[n]=0;end
        for(n=0;n<8;n=n+1) gains[n]=0;
        repeat(8) @(negedge clk);resetn=1;
        axi_write(64,14);axi_write(68,14);
        for(n=0;n<128;n=n+1) begin
            target=n%8;bank=~banks[target];
            if(n%11==0) coefficient=0;
            else if(n%13==0) coefficient=-(64'sd2048<<31);
            else if(n%7==0) coefficient=64'sd2147483647*2048;
            else coefficient=(n%2 ? -64'sd2139 : 64'sd3922) << ((n*7)%30);
            for(kind=0;kind<2;kind=kind+1)
                for(address=0;address<16;address=address+1) begin
                    factor=(kind && address>=8) ? address-16 : address;
                    transact({1'b0,target[2:0],bank,kind[0],address[3:0]},coefficient*factor,0);
                end
            transact({1'b1,target[2:0],bank,5'b0},coefficient,0);
            axi_read(16+8*target,value);
            if(value!==coefficient[31:0]) $fatal(1,"Incorrect low coefficient register");
            axi_read(20+8*target,value);
            if(value!==coefficient[63:32]) $fatal(1,"Incorrect high coefficient register");
            // Direct active-bank writes are rejected by hardware.
            transact({1'b0,target[2:0],bank,5'b0},64'sh0123456789abcdef,1);
            if(n%16==0) begin
                axi_write(64,(n%15)<<1);axi_write(68,((n+7)%15)<<1);
            end
            if(n==63) begin
                // Peripheral reset keeps committed gains and both RAM banks.
                @(negedge clk);resetn=0;request=0;
                repeat(4) @(negedge clk);resetn=1;
                axi_write(64,14);axi_write(68,14);
            end
        end
        repeat(32) @(negedge clk);
        if(commits!=128 || writes!=4096) $fatal(1,"Repeated/missing RAM writes or commits: %0d/%0d",writes,commits);
        $display("Table system checks passed: %0d cycles, two controllers, %0d acknowledged transactions, %0d single-cycle RAM writes and %0d atomic commits",checked,transactions,writes,commits);
        $finish;
    end
    initial begin #10000000;$fatal(1,"Watchdog");end
endmodule
