`timescale 1 ns / 1 ps

// Programmable constant multiplier with two/three pipeline stages. For four-bit
// chunks, software prepares sixteen multiples of the signed Q*.11 gain,
// including the octave, for unsigned nibbles and sixteen for the signed top
// nibble. Small asynchronous LUT RAMs
// replace both the coefficient multiplier and runtime octave selector.
// Write the inactive bank, then switch ACTIVE_BANK atomically.
module table_gain #(
    parameter integer A_WIDTH=32,
    parameter integer OUTPUT_LOW=0,
    parameter integer OUTPUT_WIDTH=64,
    parameter integer CHUNK_BITS=4,
    parameter integer FRACTION_BITS=11,
    parameter integer PIPE_STAGES=2,
    // Two-stage alternative: leave this many compressor levels after the
    // intermediate register, balancing LUT-read/reduction against final sum.
    parameter integer FINAL_CSA_LEVELS=0,
    // Zero uses a ripple carry chain. Positive values bound each carry-select
    // block's width, calculating its carry-zero and carry-one sums in parallel.
    parameter integer CARRY_BLOCK=0
)(
    input wire CLK,
    input wire signed [A_WIDTH-1:0] A,
    input wire ACTIVE_BANK,
    input wire WRITE_ENABLE,
    input wire WRITE_BANK,
    input wire WRITE_SIGNED,
    input wire [CHUNK_BITS-1:0] WRITE_ADDRESS,
    input wire [63:0] WRITE_DATA,
    output reg [OUTPUT_WIDTH-1:0] P=0
);
    localparam TERMS=(A_WIDTH+CHUNK_BITS-1)/CHUNK_BITS;
    localparam WIDTH=OUTPUT_LOW+OUTPUT_WIDTH+FRACTION_BITS;
    localparam TABLE_WIDTH=FRACTION_BITS+33+CHUNK_BITS;
    localparam ENTRIES=1<<(CHUNK_BITS+1);
    wire signed [CHUNK_BITS*TERMS-1:0] a_ext=A;
    function integer term_count(input integer level);
        integer k;
        begin
            term_count=TERMS;
            for(k=0;k<level;k=k+1) term_count=2*(term_count/3)+term_count%3;
        end
    endfunction
    function integer tree_depth(input integer n);
        begin
            tree_depth=0;
            while(n>2) begin n=2*(n/3)+n%3; tree_depth=tree_depth+1; end
        end
    endfunction
    localparam LEVELS=tree_depth(TERMS);
    localparam SPLIT=(FINAL_CSA_LEVELS>LEVELS) ? 0 : LEVELS-FINAL_CSA_LEVELS;
    wire [WIDTH-1:0] tree [0:(LEVELS+1)*TERMS-1];
    wire [WIDTH-1:0] stage [0:(LEVELS+1)*TERMS-1];
    genvar i,l,g,r;
    generate for(l=0;l<=LEVELS;l=l+1) begin : stage_boundary
        for(i=0;i<term_count(l);i=i+1) begin : term
            if(PIPE_STAGES==2 && FINAL_CSA_LEVELS>0 && l==SPLIT) begin : pipeline
                reg [WIDTH-1:0] value=0;
                always @(posedge CLK) value<=tree[l*TERMS+i];
                assign stage[l*TERMS+i]=value;
            end else begin
                assign stage[l*TERMS+i]=tree[l*TERMS+i];
            end
        end
    end endgenerate
    generate for(i=0;i<TERMS;i=i+1) begin : chunk
        (* ram_style="distributed" *) reg [TABLE_WIDTH-1:0] products [0:ENTRIES-1];
        integer j;
        initial for(j=0;j<ENTRIES;j=j+1) products[j]=0;
        always @(posedge CLK)
            if(WRITE_ENABLE && WRITE_SIGNED==(i==TERMS-1))
                products[{WRITE_BANK,WRITE_ADDRESS}]<=WRITE_DATA;
        wire signed [TABLE_WIDTH-1:0] lookup=products[{ACTIVE_BANK,a_ext[CHUNK_BITS*i +: CHUNK_BITS]}];
        wire signed [TABLE_WIDTH-1:0] partial;
        if(PIPE_STAGES==3) begin : lookup_stage
            reg signed [TABLE_WIDTH-1:0] value=0;
            always @(posedge CLK) value<=lookup;
            assign partial=value;
        end else begin
            assign partial=lookup;
        end
        wire signed [WIDTH-1:0] extended=partial;
        assign tree[i]=extended <<< (CHUNK_BITS*i);
    end endgenerate
    generate for(l=0;l<LEVELS;l=l+1) begin : level
        localparam N=term_count(l);
        for(g=0;g<N/3;g=g+1) begin : triple
            wire [WIDTH-1:0] x=stage[l*TERMS+3*g];
            wire [WIDTH-1:0] y=stage[l*TERMS+3*g+1];
            wire [WIDTH-1:0] z=stage[l*TERMS+3*g+2];
            assign tree[(l+1)*TERMS+2*g]=x^y^z;
            assign tree[(l+1)*TERMS+2*g+1]=((x&y)|(x&z)|(y&z))<<1;
        end
        for(r=0;r<N%3;r=r+1) begin : remainder
            assign tree[(l+1)*TERMS+2*(N/3)+r]=stage[l*TERMS+3*(N/3)+r];
        end
    end endgenerate
    generate if(PIPE_STAGES==2 && FINAL_CSA_LEVELS>0) begin : balanced
        wire [WIDTH-1:0] product;
        if(CARRY_BLOCK>0) begin : carry_select
            localparam BLOCKS=(WIDTH+CARRY_BLOCK-1)/CARRY_BLOCK;
            wire [BLOCKS:0] c;
            assign c[0]=0;
            for(i=0;i<BLOCKS;i=i+1) begin : block_sum
                localparam N=(WIDTH-i*CARRY_BLOCK<CARRY_BLOCK) ? WIDTH-i*CARRY_BLOCK : CARRY_BLOCK;
                wire [N-1:0] x=stage[LEVELS*TERMS][i*CARRY_BLOCK +: N];
                wire [N-1:0] y=stage[LEVELS*TERMS+1][i*CARRY_BLOCK +: N];
                wire [N:0] zero_carry={1'b0,x}+{1'b0,y};
                wire [N-1:0] one_carry=x+y+1'b1;
                // Carry generates within this block or propagates through it.
                assign c[i+1]=zero_carry[N] | ((&(x^y)) & c[i]);
                assign product[i*CARRY_BLOCK +: N]=c[i] ? one_carry : zero_carry[N-1:0];
            end
        end else begin
            assign product=stage[LEVELS*TERMS]+stage[LEVELS*TERMS+1];
        end
        always @(posedge CLK) P<=product[FRACTION_BITS+OUTPUT_LOW +: OUTPUT_WIDTH];
    end else begin : conventional
        reg [WIDTH-1:0] sum=0,carry=0;
        wire [WIDTH-1:0] product=sum+carry;
        always @(posedge CLK) begin
            sum<=stage[LEVELS*TERMS]; carry<=stage[LEVELS*TERMS+1];
            P<=product[FRACTION_BITS+OUTPUT_LOW +: OUTPUT_WIDTH];
        end
    end endgenerate
endmodule
