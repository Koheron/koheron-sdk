`timescale 1 ns / 1 ps

// Historical experiment; production uses table gains and manual_p_corrector.v.
// Gain = signed COEFFICIENT / 2048 * 2^OCTAVE. The host chooses one of
// round(2048 * 2^(j/12)), j=0..11, and optionally negates it or sends zero.
// Retain fractional bits until the final existing DPLL output slice.
module geometric_gain #(
    parameter integer A_WIDTH = 32,
    parameter integer OUTPUT_LOW = 0,
    parameter integer OUTPUT_WIDTH = 64,
    parameter integer PIPE_STAGES = 2,
    parameter integer DSP_OUTPUT_REG = 0,
    parameter integer DSP_SIGN_CORRECTION = 0
)(
    input wire CLK,
    input wire signed [A_WIDTH-1:0] A,
    input wire signed [12:0] COEFFICIENT,
    // Loaded atomically with COEFFICIENT by the host: -COEFFICIENT.
    input wire signed [12:0] NEG_COEFFICIENT,
    input wire [4:0] OCTAVE,
    output reg signed [OUTPUT_WIDTH-1:0] P = 0
);
    localparam PRODUCT_WIDTH = A_WIDTH + 13;
    localparam SHIFT_WIDTH = PRODUCT_WIDTH + 31;
    localparam CHUNKS = (A_WIDTH + 23) / 24;
    wire signed [24*CHUNKS-1:0] a_ext = A;
    wire signed [PRODUCT_WIDTH-1:0] term [0:CHUNKS-1];
    reg [4:0] octave1 = 0;
    always @(posedge CLK) octave1 <= OCTAVE;

    genvar i;
    generate for (i = 0; i < CHUNKS; i = i + 1) begin : chunk
        // DSP48E1 has a signed 25 x 18 multiplier. Lower chunks are
        // unsigned 24-bit values; the upper chunk is sign extended.
        wire signed [24:0] a_part = {
            (i == CHUNKS-1) && a_ext[24*i+23], a_ext[24*i +: 24]};
        localparam PART_WIDTH = (i == CHUNKS-1) ? A_WIDTH-24*i : 24;
        wire [24:0] unsigned_part = {{(25-PART_WIDTH){1'b0}}, A[24*i +: PART_WIDTH]};
        wire signed [47:0] negative_coefficient_ext = NEG_COEFFICIENT;
        // For the top chunk, signed(A)*c = unsigned(A)*c - sign(A)*2^width*c.
        // Keeping unused A bits at zero avoids sign fanout into the DSP's
        // wide A:B ALU path. PREG samples multiply + C together in one clock.
        wire signed [47:0] sign_correction = (i == CHUNKS-1 && A[A_WIDTH-1]) ?
            negative_coefficient_ext <<< PART_WIDTH : 48'sd0;
        wire signed [47:0] product;
        DSP48E1 #(
            .AREG(0), .ACASCREG(0), .BREG(0), .BCASCREG(0),
            .CREG(0), .DREG(0), .ADREG(0), .INMODEREG(0),
            .OPMODEREG(0), .ALUMODEREG(0), .CARRYINREG(0),
            .CARRYINSELREG(0), .MREG(1-DSP_OUTPUT_REG), .PREG(DSP_OUTPUT_REG)
        ) dsp (
            .CLK(CLK), .A(DSP_SIGN_CORRECTION ? {5'b0, unsigned_part} : {{5{a_part[24]}}, a_part}),
            .B({{5{COEFFICIENT[12]}}, COEFFICIENT}),
            .C(DSP_SIGN_CORRECTION ? sign_correction : 48'b0), .D(25'b0),
            .ACIN(30'b0), .BCIN(18'b0), .PCIN(48'b0),
            .OPMODE(DSP_SIGN_CORRECTION ? 7'b0110101 : 7'b0000101),
            .ALUMODE(4'b0), .INMODE(5'b0),
            .CARRYIN(1'b0), .CARRYINSEL(3'b0), .CARRYCASCIN(1'b0),
            .MULTSIGNIN(1'b0), .CEA1(1'b0), .CEA2(1'b0),
            .CEB1(1'b0), .CEB2(1'b0), .CEC(1'b0), .CED(1'b0),
            .CEAD(1'b0), .CEINMODE(1'b0), .CECTRL(1'b0),
            .CEALUMODE(1'b0), .CECARRYIN(1'b0), .CEM(!DSP_OUTPUT_REG), .CEP(DSP_OUTPUT_REG != 0),
            .RSTA(1'b0), .RSTB(1'b0), .RSTC(1'b0), .RSTD(1'b0),
            .RSTINMODE(1'b0), .RSTCTRL(1'b0), .RSTALUMODE(1'b0),
            .RSTALLCARRYIN(1'b0), .RSTM(1'b0), .RSTP(1'b0), .P(product)
        );
        // Assignment sign extends/truncates before positioning the chunk.
        wire signed [PRODUCT_WIDTH-1:0] extended = product;
        assign term[i] = extended <<< (24*i);
    end endgenerate

    wire signed [PRODUCT_WIDTH-1:0] assembled;
    generate if (CHUNKS == 1) begin
        assign assembled = term[0];
    end else begin
        assign assembled = term[0] + term[1];
    end endgenerate

    wire signed [PRODUCT_WIDTH-1:0] selected_product;
    wire [4:0] selected_octave;
    generate if (PIPE_STAGES == 3) begin : extra_stage
        reg signed [PRODUCT_WIDTH-1:0] product2 = 0;
        reg [4:0] octave2 = 0;
        always @(posedge CLK) begin
            product2 <= assembled;
            octave2 <= octave1;
        end
        assign selected_product = product2;
        assign selected_octave = octave2;
    end else begin
        assign selected_product = assembled;
        assign selected_octave = octave1;
    end endgenerate
    wire signed [SHIFT_WIDTH-1:0] extended_product = selected_product;
    wire signed [SHIFT_WIDTH-1:0] scaled = extended_product <<< selected_octave;
    always @(posedge CLK) P <= scaled >>> (11 + OUTPUT_LOW);

    initial begin
        if (A_WIDTH != 17 && A_WIDTH != 32 && A_WIDTH != 48)
            $error("Unsupported DPLL input width");
        if (PIPE_STAGES != 2 && PIPE_STAGES != 3)
            $error("Expected two or three pipeline stages");
        if (DSP_SIGN_CORRECTION && !DSP_OUTPUT_REG)
            $error("DSP sign correction needs MREG=0, PREG=1 to align C and multiplier inputs");
    end
endmodule
