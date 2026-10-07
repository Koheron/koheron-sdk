`timescale 1 ns / 1 ps
module corrector_table_test #(parameter integer CARRY_BLOCK=0);
    reg clk=0;
    always #2 clk=~clk;
    reg signed [16:0] freq=0;
    reg signed [31:0] phase=0;
    reg [2:0] enabled=0;
    reg [3:0] banks=0;
    reg [8:0] command=0;
    reg [63:0] data=0;
    reg signed [63:0] gain[0:3];
    wire [15:0] fast[0:3],slow[0:3];
    integer cycle=0,checked=0,n,j,k,c,seed=172933;
    integer first_old=-1,first_new=-1,first_two=-1,first_mixed=-1;
    reg random_run=0,measure=0;
    genvar mode;
    generate for(mode=0;mode<4;mode=mode+1) begin : implementation
        localparam STAGES=(mode>=2) ? 2 : 3;
        localparam TAIL_STAGES=(mode==2) ? 2 : 3;
        table_corrector #(.FUSED(mode!=0), .GAIN_STAGES(STAGES), .TAIL_GAIN_STAGES(TAIL_STAGES), .FINAL_CSA_LEVELS((mode>=2) ? 2 : 0), .CARRY_BLOCK((mode==2) ? CARRY_BLOCK : 0)) dut(clk,freq,phase,enabled,banks,command,data,fast[mode],slow[mode],,,);
        wire [31:0] decomposed=dut.p_correction+dut.integral_correction;
        reg [31:0] rp[0:STAGES-1],rpi[0:STAGES-1];
        reg [31:0] ri2[0:TAIL_STAGES-1];
        reg [63:0] ri3[0:TAIL_STAGES-1];
        reg [31:0] first_sum=0,second_sum=0,acc2=0;
        reg signed [47:0] acc1=0;
        reg [63:0] acc3=0;
        reg signed [127:0] product_p,product_pi,product_i2,product_i3;
        integer x;
        initial begin
            for(x=0;x<STAGES;x=x+1) begin rp[x]=0;rpi[x]=0;end
            for(x=0;x<TAIL_STAGES;x=x+1) begin ri2[x]=0;ri3[x]=0;end
        end
        always @(posedge clk) begin
            product_p=$signed(freq)*gain[0];
            product_pi=$signed(phase)*gain[1];
            product_i2=$signed(acc1)*gain[2];
            product_i3=$signed(acc2)*gain[3];
            rp[0]<=product_p[42:11];rpi[0]<=product_pi[58:27];
            ri2[0]<=product_i2[90:59];ri3[0]<=product_i3[74:11];
            for(x=1;x<STAGES;x=x+1) begin
                rp[x]<=rp[x-1];rpi[x]<=rpi[x-1];
            end
            for(x=1;x<TAIL_STAGES;x=x+1) begin
                ri2[x]<=ri2[x-1];ri3[x]<=ri3[x-1];
            end
            first_sum<=rp[STAGES-1]+rpi[STAGES-1];
            second_sum<=first_sum+ri2[TAIL_STAGES-1];
            if(!enabled[0]) acc1<=0; else acc1<=acc1+$signed(first_sum);
            if(!enabled[1]) acc2<=0;
            else if(mode) acc2<=acc2+first_sum+ri2[TAIL_STAGES-1];
            else acc2<=acc2+second_sum;
            if(!enabled[2]) acc3<=0;else acc3<=acc3+ri3[TAIL_STAGES-1];
            #1;
            if(cycle>8) begin
                if({dut.p,dut.pi,dut.i2,dut.i3,dut.acc1,dut.acc2,dut.acc3} !==
                   {rp[STAGES-1],rpi[STAGES-1],ri2[TAIL_STAGES-1],ri3[TAIL_STAGES-1],acc1,acc2,acc3})
                    $fatal(1,"Corrector arithmetic mismatch mode=%0d cycle=%0d",mode,cycle);
                if(decomposed !== dut.correction)
                    $fatal(1,"P/integral decomposition mismatch mode=%0d cycle=%0d",mode,cycle);
            end
        end
    end endgenerate
    always @(posedge clk) begin
        cycle=cycle+1;
        #1;
        if(cycle>8) checked=checked+1;
        if(measure) begin
            if(fast[0]!=0 && first_old<0) first_old=cycle;
            if(fast[1]!=0 && first_new<0) first_new=cycle;
            if(fast[2]!=0 && first_two<0) first_two=cycle;
            if(fast[3]!=0 && first_mixed<0) first_mixed=cycle;
        end
    end
    always @(negedge clk) if(random_run) begin
        freq=$random(seed);phase=$random(seed);
        enabled=(cycle%137==0) ? 0 : ((cycle%101==0) ? 3'b101 : 3'b111);
    end
    task load_gain(input integer channel,input signed [63:0] value);
        integer kind,address,factor;
        reg target;
        begin
            target=!banks[channel];
            for(kind=0;kind<2;kind=kind+1) begin
                for(address=0;address<16;address=address+1) begin
                    @(negedge clk);
                    factor=(kind && address>=8) ? address-16 : address;
                    data=value*factor;
                    command={1'b1,target,kind[0],address[3:0],channel[1:0]};
                end
            end
            @(negedge clk);
            command=0;banks[channel]=target;gain[channel]=value;
        end
    endtask
    initial begin
        for(k=0;k<4;k=k+1) gain[k]=0;
        repeat(12) @(negedge clk);
        load_gain(0,64'sd65536<<11);
        enabled=7;
        repeat(12) @(negedge clk);
        freq=1;measure=1;
        @(negedge clk);freq=0;
        repeat(16) @(negedge clk);
        if(first_old-first_new != 1)
            $fatal(1,"Expected one clock saved, observed old=%0d new=%0d",first_old,first_new);
        $display("Fast correction impulse: separate=%0d fused=%0d; one clock / 4 ns saved",first_old,first_new);
        if(first_old-first_two != 2)
            $fatal(1,"Expected two clocks saved, observed old=%0d new=%0d",first_old,first_two);
        $display("Two-cycle gain impulse: separate=%0d fused=%0d; two clocks / 8 ns saved",first_old,first_two);
        if(first_old-first_mixed != 2)
            $fatal(1,"Expected mixed-gain pipeline to save two clocks, observed old=%0d new=%0d",first_old,first_mixed);
        $display("Mixed gain impulse: separate=%0d fused=%0d; two clocks / 8 ns saved",first_old,first_mixed);
        measure=0;random_run=1;
        for(n=0;n<256;n=n+1) begin
            // Integer and fractional signed gains, including zero, overflow,
            // maximum supported octave and changes while all states evolve.
            for(c=0;c<4;c=c+1) begin
                if(n%17==0) load_gain(c,0);
                else if(n%19==0) load_gain(c,-(64'sd2048<<31));
                else load_gain(c,((n+c)%2 ? -64'sd3922 : 64'sd2139) << ((n+7*c)%32));
            end
        end
        repeat(32) @(negedge clk);
        random_run=0;
        $display("Table corrector checks passed: %0d cycles, signed products, wraparound, all integrator enables and atomic updates",checked);
        $finish;
    end
endmodule
