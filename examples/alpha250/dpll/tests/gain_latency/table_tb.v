`timescale 1 ns / 1 ps
module table_gain_test #(
    parameter integer CHUNK_BITS=6,
    parameter integer FRACTION_BITS=11,
    parameter integer PIPE_STAGES=2,
    parameter integer FINAL_CSA_LEVELS=0,
    parameter integer CARRY_BLOCK=0,
    parameter integer PHASE_FRAC=0,
    parameter integer DSP_PIPE_STAGES=3
);
    reg clk=0;
    always #2 clk=~clk;
    reg signed [47:0] a=0;
    reg active_bank=0,write_enable=0,write_bank=1,write_signed=0;
    reg [7:0] address=0;
    reg [63:0] data=0;
    wire [159:0] result;
    wire [31:0] dsp_p;
    reg [31:0] dsp_expected[0:DSP_PIPE_STAGES-1];
    integer dq;
    initial for(dq=0;dq<DSP_PIPE_STAGES;dq=dq+1) dsp_expected[dq]=0;
    table_gain #(.A_WIDTH(17),.OUTPUT_WIDTH(32),.PIPE_STAGES(DSP_PIPE_STAGES),
        .CHUNK_BITS(CHUNK_BITS),.DSP_FAST_P(1)) dsp_gain(
        clk,a[16:0],active_bank,write_enable,write_bank,write_signed,
        address[CHUNK_BITS-1:0],data,dsp_p);
    reg [159:0] expected_in=0;
    reg [31:0] dsp_expected_in=0;
    reg [159:0] expected[0:PIPE_STAGES-1];
    reg [315:0] vectors[0:599999];
    reg [1023:0] vector_path;
    integer count,cycle=0,checked=0,i;
    table_gain #(.A_WIDTH(17+PHASE_FRAC), .OUTPUT_LOW(PHASE_FRAC), .OUTPUT_WIDTH(32), .CHUNK_BITS(CHUNK_BITS), .FRACTION_BITS(FRACTION_BITS), .PIPE_STAGES(PIPE_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gp(clk,a[16+PHASE_FRAC:0],active_bank,write_enable,write_bank,write_signed,address[CHUNK_BITS-1:0],data,result[31:0]);
    table_gain #(.A_WIDTH(32+PHASE_FRAC), .OUTPUT_LOW(16+PHASE_FRAC), .OUTPUT_WIDTH(32), .CHUNK_BITS(CHUNK_BITS), .FRACTION_BITS(FRACTION_BITS), .PIPE_STAGES(PIPE_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gpi(clk,a[31+PHASE_FRAC:0],active_bank,write_enable,write_bank,write_signed,address[CHUNK_BITS-1:0],data,result[63:32]);
    table_gain #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32), .CHUNK_BITS(CHUNK_BITS), .FRACTION_BITS(FRACTION_BITS), .PIPE_STAGES(PIPE_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gi2(clk,a,active_bank,write_enable,write_bank,write_signed,address[CHUNK_BITS-1:0],data,result[95:64]);
    table_gain #(.A_WIDTH(32), .OUTPUT_WIDTH(64), .CHUNK_BITS(CHUNK_BITS), .FRACTION_BITS(FRACTION_BITS), .PIPE_STAGES(PIPE_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gi3(clk,a[31:0],active_bank,write_enable,write_bank,write_signed,address[CHUNK_BITS-1:0],data,result[159:96]);
    integer q;
    initial for(q=0;q<PIPE_STAGES;q=q+1) expected[q]=0;
    always @(posedge clk) begin
        expected[0]<=expected_in;
        dsp_expected[0]<=dsp_expected_in;
        for(dq=1;dq<DSP_PIPE_STAGES;dq=dq+1) dsp_expected[dq]<=dsp_expected[dq-1];
        for(q=1;q<PIPE_STAGES;q=q+1) expected[q]<=expected[q-1];
        cycle=cycle+1;
        #1;
        if(cycle>4) begin
            if(result !== expected[PIPE_STAGES-1])
                $fatal(1,"Table gain mismatch cycle %0d: %h expected %h",cycle,result,expected[PIPE_STAGES-1]);
            if(FRACTION_BITS==11 && dsp_p!==dsp_expected[DSP_PIPE_STAGES-1])
                $fatal(1,"DSP Fast P gain mismatch cycle %0d: %h expected %h",cycle,dsp_p,dsp_expected[DSP_PIPE_STAGES-1]);
            checked=checked+1;
        end
    end
    initial begin
        if(!$value$plusargs("vectors=%s",vector_path) || !$value$plusargs("count=%d",count))
            $fatal(1,"Expected vectors and count plusargs");
        $readmemh(vector_path,vectors,0,count-1);
        repeat(4) @(negedge clk);
        for(i=0;i<count;i=i+1) begin
            @(negedge clk);
            {a,write_enable,write_bank,write_signed,address,data,active_bank,expected_in,dsp_expected_in}=vectors[i];
        end
        repeat(4) @(negedge clk);
        $display("Table gain checks passed: %0d cycles, exact %0d-clock latency, background writes, atomic gain updates and registered-input DSP Fast P",checked,PIPE_STAGES);
        $finish;
    end
endmodule
