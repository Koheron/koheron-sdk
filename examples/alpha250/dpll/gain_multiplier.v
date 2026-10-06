`timescale 1 ns / 1 ps

// DPLL's 17/32/48-bit signed inputs times a 32-bit gain in three clocks:
// parallel DSP products, carry-save
// reduction, then one carry-propagating sum. Splitting the operands avoids
// the long unregistered DSP cascade of a wide mult_gen with three stages.
module gain_multiplier #(
    parameter integer A_WIDTH = 32,
    parameter integer B_WIDTH = 32,
    parameter integer OUTPUT_LOW = 0,
    parameter integer OUTPUT_WIDTH = A_WIDTH + B_WIDTH
)(
    input wire CLK,
    input wire signed [A_WIDTH-1:0] A,
    input wire signed [B_WIDTH-1:0] B,
    output reg signed [OUTPUT_WIDTH-1:0] P = 0
);
    localparam NA = (A_WIDTH + 15) / 16;
    localparam NB = (B_WIDTH + 15) / 16;
    localparam TERMS = NA * NB;
    localparam WIDTH = 16 * (NA + NB);
    wire signed [16*NA-1:0] a_ext = A;
    wire signed [16*NB-1:0] b_ext = B;
    wire [WIDTH-1:0] term [0:TERMS-1];

    genvar ai, bi;
    generate
        for (ai = 0; ai < NA; ai = ai + 1) begin : a_chunk
            for (bi = 0; bi < NB; bi = bi + 1) begin : b_chunk
                // Only the top chunk is signed; lower chunks are unsigned.
                wire signed [16:0] a_part = {(ai == NA-1) && a_ext[16*ai+15], a_ext[16*ai +: 16]};
                wire signed [16:0] b_part = {(bi == NB-1) && b_ext[16*bi+15], b_ext[16*bi +: 16]};
                wire [47:0] product;
                // Use MREG for the first stage. PREG would put the multiplier
                // and its output adder before the register, with a longer
                // input setup time. The adder now shares the carry-save stage.
                DSP48E1 #(
                    .AREG(0), .ACASCREG(0), .BREG(0), .BCASCREG(0),
                    .CREG(0), .DREG(0), .ADREG(0), .INMODEREG(0),
                    .OPMODEREG(0), .ALUMODEREG(0), .CARRYINREG(0),
                    .CARRYINSELREG(0), .MREG(1), .PREG(0)
                ) dsp (
                    .CLK(CLK), .A({{13{a_part[16]}}, a_part}),
                    .B({b_part[16], b_part}), .C(48'b0), .D(25'b0),
                    .ACIN(30'b0), .BCIN(18'b0), .PCIN(48'b0),
                    .OPMODE(7'b0000101), .ALUMODE(4'b0), .INMODE(5'b0),
                    .CARRYIN(1'b0), .CARRYINSEL(3'b0), .CARRYCASCIN(1'b0),
                    .MULTSIGNIN(1'b0), .CEA1(1'b0), .CEA2(1'b0),
                    .CEB1(1'b0), .CEB2(1'b0), .CEC(1'b0), .CED(1'b0),
                    .CEAD(1'b0), .CEINMODE(1'b0), .CECTRL(1'b0),
                    .CEALUMODE(1'b0), .CECARRYIN(1'b0), .CEM(1'b1), .CEP(1'b0),
                    .RSTA(1'b0), .RSTB(1'b0), .RSTC(1'b0), .RSTD(1'b0),
                    .RSTINMODE(1'b0), .RSTCTRL(1'b0), .RSTALUMODE(1'b0),
                    .RSTALLCARRYIN(1'b0), .RSTM(1'b0), .RSTP(1'b0), .P(product)
                );
                // Unsigned 16x16 fits 32 unsigned bits; every other chunk
                // pair fits 32 signed bits. Expose these exact zero/sign bits
                // rather than asking synthesis to infer them through a DSP.
                wire signed [WIDTH-1:0] extended;
                if (ai < NA-1 && bi < NB-1)
                    assign extended = {{(WIDTH-32){1'b0}}, product[31:0]};
                else
                    assign extended = $signed(product[31:0]);
                assign term[ai*NB+bi] = extended << (16 * (ai + bi));
            end
        end
    endgenerate

    // Reduce independent triples in parallel at each level. This takes three
    // levels for six products instead of four serial carry-save reductions.
    function integer term_count(input integer level);
        integer k;
        begin
            term_count = TERMS;
            for (k = 0; k < level; k = k + 1)
                term_count = 2 * (term_count / 3) + term_count % 3;
        end
    endfunction
    function integer tree_depth(input integer n);
        begin
            tree_depth = 0;
            while (n > 2) begin
                n = 2 * (n / 3) + n % 3;
                tree_depth = tree_depth + 1;
            end
        end
    endfunction
    localparam LEVELS = tree_depth(TERMS);
    wire [WIDTH-1:0] tree [0:(LEVELS+1)*TERMS-1];
    genvar level, group_index, remainder_index, term_index;
    generate
        for (term_index = 0; term_index < TERMS; term_index = term_index + 1)
            assign tree[term_index] = term[term_index];
        for (level = 0; level < LEVELS; level = level + 1) begin : reduction
            localparam COUNT = term_count(level);
            for (group_index = 0; group_index < COUNT/3; group_index = group_index + 1) begin : triple
                wire [WIDTH-1:0] x = tree[level*TERMS + 3*group_index];
                wire [WIDTH-1:0] y = tree[level*TERMS + 3*group_index + 1];
                wire [WIDTH-1:0] z = tree[level*TERMS + 3*group_index + 2];
                assign tree[(level+1)*TERMS + 2*group_index] = x ^ y ^ z;
                assign tree[(level+1)*TERMS + 2*group_index + 1] = ((x & y) | (x & z) | (y & z)) << 1;
            end
            for (remainder_index = 0; remainder_index < COUNT%3; remainder_index = remainder_index + 1)
                assign tree[(level+1)*TERMS + 2*(COUNT/3) + remainder_index] =
                    tree[level*TERMS + 3*(COUNT/3) + remainder_index];
        end
    endgenerate
    reg [WIDTH-1:0] sum_reg = 0, carry_reg = 0;
    wire [WIDTH-1:0] full_product = sum_reg + carry_reg;
    always @(posedge CLK) begin
        sum_reg <= tree[LEVELS*TERMS];
        carry_reg <= tree[LEVELS*TERMS + 1];
        P <= full_product[OUTPUT_LOW +: OUTPUT_WIDTH];
    end
endmodule
