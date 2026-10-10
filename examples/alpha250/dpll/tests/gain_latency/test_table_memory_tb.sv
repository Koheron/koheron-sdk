`timescale 1ns/1ps
// Arbitrary table contents exercise RAM semantics independently of coefficient
// generation, including same-address reads/writes and bank changes every clock.
module table_memory_check #(
    parameter A_WIDTH=40, OUTPUT_LOW=24, OUTPUT_WIDTH=32,
    parameter PIPE_STAGES=4, CHUNK_BITS=4, FINAL_CSA_LEVELS=2
)(input clk, output reg done=0);
    localparam TERMS=(A_WIDTH+CHUNK_BITS-1)/CHUNK_BITS;
    localparam TABLE_WIDTH=11+33+CHUNK_BITS;
    localparam ENTRIES=1<<CHUNK_BITS;
    reg signed [A_WIDTH-1:0] sample=0;
    reg bank=0,we=0,write_bank=0,write_signed=0;
    reg [CHUNK_BITS-1:0] address=0;
    reg [63:0] data=0;
    wire [OUTPUT_WIDTH-1:0] result,original_result;
    table_gain #(.A_WIDTH(A_WIDTH),.OUTPUT_LOW(OUTPUT_LOW),.OUTPUT_WIDTH(OUTPUT_WIDTH),
        .PIPE_STAGES(PIPE_STAGES),.FINAL_CSA_LEVELS(FINAL_CSA_LEVELS),.CHUNK_BITS(CHUNK_BITS),.REGISTER_ADDRESS(PIPE_STAGES>=4)) dut(
        clk,sample,bank,we,write_bank,write_signed,address,data,result);
    table_gain #(.A_WIDTH(A_WIDTH),.OUTPUT_LOW(OUTPUT_LOW),.OUTPUT_WIDTH(OUTPUT_WIDTH),
        .PIPE_STAGES(PIPE_STAGES),.FINAL_CSA_LEVELS(FINAL_CSA_LEVELS),.CHUNK_BITS(CHUNK_BITS)) original(
        clk,sample,bank,we,write_bank,write_signed,address,data,original_result);
    reg signed [TABLE_WIDTH-1:0] reference_table[0:2*2*ENTRIES-1];
    reg [OUTPUT_WIDTH-1:0] expected[0:PIPE_STAGES-1];
    reg signed [CHUNK_BITS*TERMS-1:0] extended;
    reg signed [127:0] term,total;
    integer index,k,cycle=0,writes=0,collisions=0;
    initial begin
        for(k=0;k<4*ENTRIES;k=k+1) reference_table[k]=0;
        for(k=0;k<PIPE_STAGES;k=k+1) expected[k]=0;
    end
    always @(posedge clk) if(!done) begin
        extended=sample;
        total=0;
        for(k=0;k<TERMS;k=k+1) begin
            index=((k==TERMS-1)*2+bank)*ENTRIES+extended[CHUNK_BITS*k +: CHUNK_BITS];
            term=reference_table[index];
            total=total+(term<<<(CHUNK_BITS*k));
            if(we && write_bank==bank && write_signed==(k==TERMS-1) &&
               address==extended[CHUNK_BITS*k +: CHUNK_BITS]) collisions=collisions+1;
        end
        // Read the old word before applying this clock's reference write.
        expected[0]<=total>>>(11+OUTPUT_LOW);
        for(k=1;k<PIPE_STAGES;k=k+1) expected[k]<=expected[k-1];
        if(we) begin
            reference_table[(write_signed*2+write_bank)*ENTRIES+address]=data;
            writes=writes+1;
        end
        cycle=cycle+1;
        #1;
        if(result!==expected[PIPE_STAGES-1] || original_result!==expected[PIPE_STAGES-1])
            $fatal(1,"Table memory mismatch A=%0d stages=%0d chunk=%0d cycle=%0d result=%h original=%h expected=%h",
                A_WIDTH,PIPE_STAGES,CHUNK_BITS,cycle,result,original_result,expected[PIPE_STAGES-1]);
    end
    integer n,seed=948571+A_WIDTH+PIPE_STAGES+CHUNK_BITS;
    initial begin
        for(n=0;n<12000;n=n+1) begin
            @(negedge clk);
            bank=$random(seed);we=(n%7!=0);write_bank=$random(seed);write_signed=$random(seed);
            address=$random(seed);data={$random(seed),$random(seed)};
            sample={$random(seed),$random(seed)};
            // Repeated collisions, arbitrary signed words, rapid bank changes.
            if(n%3==0) begin
                write_bank=bank;
                sample={TERMS{address}};
            end
        end
        we=0;
        repeat(PIPE_STAGES+1) @(negedge clk);
        if(writes<10000 || collisions<500) $fatal(1,"Insufficient write/collision coverage");
        done=1;
        $display("Table memory checks passed: A=%0d stages=%0d chunk=%0d final_levels=%0d cycles=%0d writes=%0d collisions=%0d",
            A_WIDTH,PIPE_STAGES,CHUNK_BITS,FINAL_CSA_LEVELS,cycle,writes,collisions);
    end
endmodule

module test_table_memory_tb;
    reg clk=0;
    always #2 clk=~clk;
    wire [7:0] done;
    table_memory_check #(.A_WIDTH(17),.OUTPUT_LOW(0),.PIPE_STAGES(2)) p2(clk,done[0]);
    table_memory_check #(.A_WIDTH(40),.PIPE_STAGES(3)) p3(clk,done[1]);
    table_memory_check #(.A_WIDTH(40),.PIPE_STAGES(4)) p4(clk,done[2]);
    table_memory_check #(.A_WIDTH(48),.OUTPUT_LOW(48),.PIPE_STAGES(5)) p5(clk,done[3]);
    table_memory_check #(.A_WIDTH(32),.OUTPUT_LOW(0),.OUTPUT_WIDTH(64),.CHUNK_BITS(3)) c3(clk,done[4]);
    table_memory_check #(.A_WIDTH(40),.CHUNK_BITS(6)) c6(clk,done[5]);
    table_memory_check #(.A_WIDTH(40),.FINAL_CSA_LEVELS(3)) balanced4(clk,done[6]);
    table_memory_check #(.A_WIDTH(48),.OUTPUT_LOW(48),.PIPE_STAGES(5),.FINAL_CSA_LEVELS(3)) balanced5(clk,done[7]);
    initial begin wait(&done);$finish;end
    initial begin #1000000;$fatal(1,"Watchdog");end
endmodule
