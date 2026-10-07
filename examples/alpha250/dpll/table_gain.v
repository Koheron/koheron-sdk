`timescale 1 ns / 1 ps

// Programmable constant multiplier with two to four pipeline stages. For four-bit
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
    parameter integer DSP_FAST_P=0,
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
    generate if(DSP_FAST_P) begin : dsp_p
        // The unsigned address-one table entry is the coefficient itself.
        // Capture it in each bank, retaining the existing programming protocol.
        if(A_WIDTH!=17 || OUTPUT_LOW!=0 || OUTPUT_WIDTH!=32 || FRACTION_BITS!=11 || (PIPE_STAGES!=2 && PIPE_STAGES!=3))
            initial $error("DSP Fast P requires the 17-bit Q11 P interface");
        reg [42:0] coefficients[0:1];
        initial begin coefficients[0]=0; coefficients[1]=0; end
        always @(posedge CLK)
            if(WRITE_ENABLE && !WRITE_SIGNED && WRITE_ADDRESS==1)
                coefficients[WRITE_BANK]<=WRITE_DATA[42:0];
        wire [42:0] coefficient=coefficients[ACTIVE_BANK];
        wire signed [24:0] low_coefficient={1'b0,coefficient[23:0]};
        wire signed [18:0] high_coefficient=coefficient[42:24];
        if(PIPE_STAGES==3) begin : registered_inputs
            // A/B input registers capture the combinational projection one
            // clock before its status/I register. P's total latency is unchanged.
            wire [47:0] low_product,high_product;
            dpll_p_product low_gain(CLK,low_coefficient,A,low_product);
            dpll_p_product high_gain(CLK,{{6{high_coefficient[18]}},high_coefficient},A,high_product);
            always @(posedge CLK) P<=low_product[42:11]+{high_product[18:0],13'b0};
        end else begin : direct_inputs
        reg signed [41:0] low_product=0;
        reg signed [35:0] high_product=0;
        // Both products fit one DSP. Only nineteen upper output bits need an
        // addition: the shifted high product has thirteen zero low bits.
        always @(posedge CLK) begin
            low_product<=A*low_coefficient;
            high_product<=A*high_coefficient;
            P<={low_product[41],low_product[41:11]}+{high_product[18:0],13'b0};
        end
        end
    end else begin : tables
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
    for(l=0;l<=LEVELS;l=l+1) begin : stage_boundary
        for(i=0;i<term_count(l);i=i+1) begin : term
            if((PIPE_STAGES==4 || (PIPE_STAGES>=2 && FINAL_CSA_LEVELS>0)) && l==SPLIT) begin : pipeline
                reg [WIDTH-1:0] value=0;
                always @(posedge CLK) value<=tree[l*TERMS+i];
                assign stage[l*TERMS+i]=value;
            end else begin
                assign stage[l*TERMS+i]=tree[l*TERMS+i];
            end
        end
    end
    for(i=0;i<TERMS;i=i+1) begin : chunk
        (* ram_style="distributed" *) reg [TABLE_WIDTH-1:0] products [0:ENTRIES-1];
        integer j;
        initial for(j=0;j<ENTRIES;j=j+1) products[j]=0;
        always @(posedge CLK)
            if(WRITE_ENABLE && WRITE_SIGNED==(i==TERMS-1))
                products[{WRITE_BANK,WRITE_ADDRESS}]<=WRITE_DATA;
        wire signed [TABLE_WIDTH-1:0] lookup=products[{ACTIVE_BANK,a_ext[CHUNK_BITS*i +: CHUNK_BITS]}];
        wire signed [TABLE_WIDTH-1:0] partial;
        if(PIPE_STAGES>=3) begin : lookup_stage
            reg signed [TABLE_WIDTH-1:0] value=0;
            always @(posedge CLK) value<=lookup;
            assign partial=value;
        end else begin
            assign partial=lookup;
        end
        wire signed [WIDTH-1:0] extended=partial;
        assign tree[i]=extended <<< (CHUNK_BITS*i);
    end
    for(l=0;l<LEVELS;l=l+1) begin : level
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
    end
    if(((PIPE_STAGES==2 || PIPE_STAGES==3) && FINAL_CSA_LEVELS>0)) begin : balanced
        wire [WIDTH-1:0] product;
        dpll_carry_adder #(.WIDTH(WIDTH),.BLOCK(CARRY_BLOCK)) final_add(
            stage[LEVELS*TERMS],stage[LEVELS*TERMS+1],1'b0,product);
        always @(posedge CLK) P<=product[FRACTION_BITS+OUTPUT_LOW +: OUTPUT_WIDTH];
    end else if(PIPE_STAGES==4 && FINAL_CSA_LEVELS>0 && (FRACTION_BITS+OUTPUT_LOW)>0) begin : split_final
        // Lower bits affect the retained output only through their carry.
        // Compute that carry at the existing intermediate register boundary.
        localparam CUT=(FRACTION_BITS+OUTPUT_LOW<32) ? FRACTION_BITS+OUTPUT_LOW : 32;
        reg [WIDTH-CUT-1:0] sum=0,carry=0;
        reg low_carry=0;
        wire [CUT:0] low_total={1'b0,stage[LEVELS*TERMS][CUT-1:0]}+
                                  {1'b0,stage[LEVELS*TERMS+1][CUT-1:0]};
        wire [WIDTH-CUT-1:0] product;
        dpll_carry_adder #(.WIDTH(WIDTH-CUT),.BLOCK(CARRY_BLOCK)) final_add(sum,carry,low_carry,product);
        always @(posedge CLK) begin
            sum<=stage[LEVELS*TERMS][WIDTH-1:CUT];
            carry<=stage[LEVELS*TERMS+1][WIDTH-1:CUT];
            low_carry<=low_total[CUT];
            P<=product[FRACTION_BITS+OUTPUT_LOW-CUT +: OUTPUT_WIDTH];
        end
    end else begin : conventional
        reg [WIDTH-1:0] sum=0,carry=0;
        wire [WIDTH-1:0] product;
        dpll_carry_adder #(.WIDTH(WIDTH),.BLOCK(CARRY_BLOCK)) final_add(sum,carry,1'b0,product);
        always @(posedge CLK) begin
            sum<=stage[LEVELS*TERMS]; carry<=stage[LEVELS*TERMS+1];
            P<=product[FRACTION_BITS+OUTPUT_LOW +: OUTPUT_WIDTH];
        end
    end
    end endgenerate
endmodule

// Short local carry chains, connected by the 7-series dedicated carry fabric.
// Selecting this implementation adds no registers or sample delay.
module dpll_carry_adder #(
    parameter integer WIDTH=32,
    parameter integer BLOCK=0
)(input wire [WIDTH-1:0] x,y, input wire cin, output wire [WIDTH-1:0] sum);
    generate if(BLOCK==0) begin : ripple
        assign sum=x+y+cin;
    end else begin : bounded
        localparam BLOCKS=(WIDTH+BLOCK-1)/BLOCK;
        localparam GROUPS=(BLOCKS+3)/4;
        wire [GROUPS*4-1:0] g,p;
        wire [GROUPS*4:0] c;
        assign c[0]=cin;
        genvar i;
        for(i=0;i<BLOCKS;i=i+1) begin : chunk
            localparam N=(WIDTH-i*BLOCK<BLOCK) ? WIDTH-i*BLOCK : BLOCK;
            wire [N-1:0] a=x[i*BLOCK +: N],b=y[i*BLOCK +: N];
            wire [N:0] zero={1'b0,a}+{1'b0,b};
            wire [N-1:0] one=a+b+1'b1;
            assign g[i]=zero[N]; assign p[i]=&(a^b);
            assign sum[i*BLOCK +: N]=c[i] ? one : zero[N-1:0];
        end
        for(i=BLOCKS;i<GROUPS*4;i=i+1) begin : pad
            assign g[i]=0; assign p[i]=0;
        end
        for(i=0;i<GROUPS;i=i+1) begin : carry_group
            CARRY4 chain(.CI(i==0 ? 1'b0 : c[4*i]),.CYINIT(i==0 ? cin : 1'b0),.DI(g[4*i +: 4]),.S(p[4*i +: 4]),
                         .CO(c[4*i+1 +: 4]),.O());
        end
    end endgenerate
endmodule

// Explicit input and output registers keep the DSP multiplier setup off the
// external coefficient mux and projection logic. This is a two-clock product.
module dpll_p_product(input wire clk, input wire signed [24:0] coefficient,
    input wire signed [16:0] sample, output wire [47:0] product);
    DSP48E1 #(.AREG(1),.ACASCREG(1),.BREG(1),.BCASCREG(1),.CREG(0),.DREG(0),
        .ADREG(0),.MREG(0),.PREG(1),.ALUMODEREG(0),.OPMODEREG(0),.INMODEREG(0),
        .CARRYINREG(0),.CARRYINSELREG(0),.USE_MULT("MULTIPLY"),.USE_SIMD("ONE48")) dsp(
        .CLK(clk),.A({{5{coefficient[24]}},coefficient}),.B({sample[16],sample}),
        .C(48'b0),.D(25'b0),.ACIN(30'b0),.BCIN(18'b0),.PCIN(48'b0),
        .ALUMODE(4'b0),.INMODE(5'b0),.OPMODE(7'b0000101),.CARRYIN(1'b0),
        .CARRYINSEL(3'b0),.CARRYCASCIN(1'b0),.MULTSIGNIN(1'b0),
        .CEA1(1'b1),.CEA2(1'b1),.CEB1(1'b1),.CEB2(1'b1),.CEAD(1'b0),
        .CEC(1'b0),.CED(1'b0),.CEM(1'b0),.CEP(1'b1),.CEALUMODE(1'b0),
        .CECTRL(1'b0),.CEINMODE(1'b0),.CECARRYIN(1'b0),
        .RSTA(1'b0),.RSTB(1'b0),.RSTC(1'b0),.RSTD(1'b0),.RSTM(1'b0),.RSTP(1'b0),
        .RSTCTRL(1'b0),.RSTINMODE(1'b0),.RSTALUMODE(1'b0),.RSTALLCARRYIN(1'b0),.P(product));
endmodule
