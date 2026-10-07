`timescale 1 ns / 1 ps
module test_p_detector_tb;
    reg clk=0, resetn=0, request=0;
    always #2 clk=~clk;
    reg signed [15:0] i_in=0,q_in=0;
    reg signed [24:0] cx=0,cy=0;
    wire signed [16:0] phase;
    wire in_range, ack;
    wire [31:0] snapshot;
    fast_p_detector dut(clk,i_in,q_in,cx,cy,phase,in_range,);
    reg signed [15:0] capture_i=0,capture_q=0;
    p_reference_capture capture(clk,resetn,request,capture_i,capture_q,ack,snapshot,32'sd0,);
    integer expected=0, valid=0, tolerance=0;
    integer ep0=0,ep1=0,ep2=0,ep3=0,vp0=0,vp1=0,vp2=0,vp3=0,vp4=0,vp5=0,tp0=0,tp1=0,tp2=0,tp3=0;
    integer cycle=0,checked=0,f,scan,k;
    integer a,b,c,d,e,v,t;
    reg [1023:0] vectors;
    always @(posedge clk) begin
        ep0<=expected; ep1<=ep0; ep2<=ep1; ep3<=ep2;
        vp0<=valid; vp1<=vp0; vp2<=vp1;vp3<=vp2;vp4<=vp3;vp5<=vp4;
        tp0<=tolerance; tp1<=tp0; tp2<=tp1;tp3<=tp2;
        cycle=cycle+1;
        #1;
        if(cycle>5) begin
            if($signed(phase)>ep3+tp3 || $signed(phase)<ep3-tp3 || in_range!==vp5[0])
                $fatal(1,"P projection cycle=%0d expected=%0d got=%0d range=%0d/%0d",cycle,ep3,$signed(phase),vp5,in_range);
            checked=checked+1;
        end
    end
    initial begin
        if(!$value$plusargs("VECTORS=%s",vectors)) $fatal(1,"Missing vectors");
        f=$fopen(vectors,"r"); if(!f) $fatal(1,"Cannot open vectors");
        scan=$fscanf(f,"%d %d %d %d %d %d %d\n",a,b,c,d,e,v,t);
        while(scan==7) begin
            @(negedge clk);
            cx=a;cy=b;i_in=c;q_in=d;expected=e;valid=v;tolerance=t;
            scan=$fscanf(f,"%d %d %d %d %d %d %d\n",a,b,c,d,e,v,t);
        end
        $fclose(f);
        repeat(4) @(negedge clk);
        resetn=1; request=1;
        @(posedge clk); #1;
        if(ack) $fatal(1,"Capture acknowledged before data");
        for(k=0;k<64;k=k+1) begin
            @(negedge clk);capture_i=1000+k;capture_q=-500-2*k;
            @(posedge clk); #1;
            if(k<63 && ack) $fatal(1,"Early capture acknowledgement");
        end
        repeat(2) begin @(posedge clk); #1; end
        if(!ack || snapshot!=={16'hfdcd,16'd1031})
            $fatal(1,"Capture average expected=(1031,-563) got=%h",snapshot);
        capture_i=-32768; capture_q=32767;
        repeat(70) @(negedge clk);
        if(snapshot!=={16'hfdcd,16'd1031}) $fatal(1,"Held request rewrote snapshot");
        request=0;
        repeat(68) @(negedge clk);
        if(ack || snapshot!==32'h7fff8000) $fatal(1,"Capture signed limits failed");
        request=1; repeat(4) @(negedge clk); resetn=0;
        @(negedge clk);
        if(ack || snapshot) $fatal(1,"Capture reset failed");
        $display("P detector checks passed: %0d cycles, four-clock projection, coherent 64-sample captures",checked);
        $finish;
    end
    initial begin #1000000; $fatal(1,"P detector timeout"); end
endmodule
