`timescale 1 ns / 1 ps
module latched_mux_tb #(
    parameter integer WIDTH=8,
    parameter integer N_INPUTS=3,
    parameter integer SEL_WIDTH=2,
    parameter integer OUTPUT_STAGES=1
);
    reg clk=0,clken=0;
    always #2 clk=~clk;
    reg [N_INPUTS*WIDTH-1:0] din=0;
    reg [SEL_WIDTH-1:0] sel=0;
    wire [WIDTH-1:0] dout;
    latched_mux #(.WIDTH(WIDTH),.N_INPUTS(N_INPUTS),.SEL_WIDTH(SEL_WIDTH),
        .OUTPUT_STAGES(OUTPUT_STAGES)) dut(clk,clken,din,sel,dout);
    reg [WIDTH-1:0] history[0:OUTPUT_STAGES-1];
    integer selected_index=0,cycles=0,checked=0,j,k,n,seed=32813;
    initial for(j=0;j<OUTPUT_STAGES;j=j+1) history[j]=0;
    always @(posedge clk) begin
        for(j=OUTPUT_STAGES-1;j>0;j=j-1) history[j]=history[j-1];
        history[0]=din[selected_index*WIDTH +: WIDTH];
        if(clken) selected_index=sel;
        cycles=cycles+1;
        #1;
        if(cycles>=OUTPUT_STAGES) begin
            if(dout!==history[OUTPUT_STAGES-1])
                $fatal(1,"Mux output/latency mismatch cycle=%0d stages=%0d",cycles,OUTPUT_STAGES);
            checked=checked+1;
        end
    end
    initial begin
        for(k=0;k<10000;k=k+1) begin
            @(negedge clk);
            for(n=0;n<N_INPUTS;n=n+1) din[n*WIDTH +: WIDTH]=$random(seed);
            sel=k%N_INPUTS;
            clken=(k%7!=0 && k%11!=0);
        end
        repeat(OUTPUT_STAGES+1) @(negedge clk);
        $display("Latched mux checks passed: %0d cycles, exact %0d-clock output, all selections and held control",checked,OUTPUT_STAGES);
        $finish;
    end
endmodule
