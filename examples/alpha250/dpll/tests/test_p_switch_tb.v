`timescale 1 ns / 1 ps
module test_p_switch_tb;
    reg clk=0, resetn=0, enabled=0, request=0;
    always #2 clk=~clk;
    reg [31:0] accurate=0,integral=0,fast_p=0,fast_i=0;
    wire [31:0] correction;
    wire active;
    p_path_switch #(.CARRY_BLOCK(0),.PRECOMBINE_I(1)) dut(clk,resetn,enabled,request,accurate,integral,fast_p,correction,active,fast_i);
    reg [31:0] previous_output=0, previous_source=0, source;
    reg previous_mode=0, previous_enabled=0;
    reg [31:0] mixed_reference=0,mixed_before;
    integer cycle=0,checked=0,transitions=0,seed=1439,k;
    always @(posedge clk) begin
        previous_output=correction; previous_mode=active;
        mixed_before=mixed_reference;mixed_reference=integral+fast_i;
        #1;
        source=active ? mixed_before+fast_p : accurate;
        if(!resetn || !enabled) begin
            if(correction || active) $fatal(1,"Disabled output/state not cleared");
        end else if(active!=previous_mode) begin
            if(correction!==previous_output) $fatal(1,"P mode transition jumped");
            transitions=transitions+1;
        end else if(previous_enabled) begin
            // Once selected, output increments must exactly follow that source
            // independent of constant offset and unsigned modular wraparound.
            if(correction-previous_output !== source-previous_source)
                $fatal(1,"P source increment mismatch cycle=%0d",cycle);
        end
        previous_source=source; previous_enabled=resetn && enabled;
        checked=checked+1;
    end
    initial begin
        repeat(4) @(negedge clk); resetn=1; enabled=1;
        for(k=0;k<20000;k=k+1) begin
            @(negedge clk); cycle=cycle+1;
            accurate=$random(seed);integral=$random(seed);fast_p=$random(seed);fast_i=$random(seed);
            if(k%71==0) request=~request;
            if(k%109==0) enabled=0; else enabled=1;
            if(k%997==0) resetn=0; else resetn=1;
        end
        @(negedge clk);
        if(transitions<100) $fatal(1,"Insufficient handoffs");
        $display("P switch checks passed: %0d cycles, %0d jump-free handoffs",checked,transitions);
        $finish;
    end
endmodule
