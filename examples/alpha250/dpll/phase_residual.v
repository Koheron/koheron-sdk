`timescale 1 ns / 1 ps

// Three-clock small-angle completion after eight vectoring rotations.
// x is positive, with leading bit 23..25; |y/x| is approximately <= 2^-7.
// atan(y/x) is replaced by y/x, with <= 0.160 urad approximation error at
// that bound. An interpolated reciprocal includes the scaled-radian factor.
module phase_residual (
    input wire clk,
    input wire resetn,
    input wire valid_in,
    input wire zero_in,
    input wire signed [26:0] x_in,
    input wire signed [20:0] y_in,
    input wire signed [31:0] angle_in,
    output wire valid_out,
    output wire signed [23:0] phase_out
);
    wire [1:0] scale = x_in[25] ? 2'd2 : x_in[24] ? 2'd1 : 2'd0;
    wire [19:0] mantissa = x_in[25] ? x_in[25:6] :
                           x_in[24] ? x_in[24:5] : x_in[23:4];
    // Each word packs round(2^18/(pi*m)) and its decrement over the next
    // 1/256 interval. The remaining 11 mantissa bits interpolate within it.
    (* rom_style = "distributed" *) reg [25:0] reciprocal_rom [0:255];
    initial begin
        reciprocal_rom[0] = 26'h28be745;
        reciprocal_rom[1] = 26'h2895d42;
        reciprocal_rom[2] = 26'h286d93f;
        reciprocal_rom[3] = 26'h2845b3e;
        reciprocal_rom[4] = 26'h281df3a;
        reciprocal_rom[5] = 26'h27f6b39;
        reciprocal_rom[6] = 26'h27cf936;
        reciprocal_rom[7] = 26'h27a8d34;
        reciprocal_rom[8] = 26'h2782531;
        reciprocal_rom[9] = 26'h275c32f;
        reciprocal_rom[10] = 26'h273652d;
        reciprocal_rom[11] = 26'h2710b2a;
        reciprocal_rom[12] = 26'h26eb729;
        reciprocal_rom[13] = 26'h26c6526;
        reciprocal_rom[14] = 26'h26a1924;
        reciprocal_rom[15] = 26'h267d121;
        reciprocal_rom[16] = 26'h2658f20;
        reciprocal_rom[17] = 26'h2634f1e;
        reciprocal_rom[18] = 26'h261131b;
        reciprocal_rom[19] = 26'h25edd1a;
        reciprocal_rom[20] = 26'h25ca917;
        reciprocal_rom[21] = 26'h25a7b15;
        reciprocal_rom[22] = 26'h2585114;
        reciprocal_rom[23] = 26'h2562911;
        reciprocal_rom[24] = 26'h2540710;
        reciprocal_rom[25] = 26'h251e70d;
        reciprocal_rom[26] = 26'h24fcd0c;
        reciprocal_rom[27] = 26'h24db50a;
        reciprocal_rom[28] = 26'h24ba108;
        reciprocal_rom[29] = 26'h2499106;
        reciprocal_rom[30] = 26'h2478504;
        reciprocal_rom[31] = 26'h2457d02;
        reciprocal_rom[32] = 26'h2437901;
        reciprocal_rom[33] = 26'h24176ff;
        reciprocal_rom[34] = 26'h23f78fd;
        reciprocal_rom[35] = 26'h23d7efb;
        reciprocal_rom[36] = 26'h23b88fa;
        reciprocal_rom[37] = 26'h23994f8;
        reciprocal_rom[38] = 26'h237a4f6;
        reciprocal_rom[39] = 26'h235b8f5;
        reciprocal_rom[40] = 26'h233cef3;
        reciprocal_rom[41] = 26'h231e8f1;
        reciprocal_rom[42] = 26'h23006f0;
        reciprocal_rom[43] = 26'h22e26ee;
        reciprocal_rom[44] = 26'h22c4aed;
        reciprocal_rom[45] = 26'h22a70eb;
        reciprocal_rom[46] = 26'h2289ae9;
        reciprocal_rom[47] = 26'h226c8e8;
        reciprocal_rom[48] = 26'h224f8e7;
        reciprocal_rom[49] = 26'h2232ae4;
        reciprocal_rom[50] = 26'h22162e4;
        reciprocal_rom[51] = 26'h21f9ae2;
        reciprocal_rom[52] = 26'h21dd6e0;
        reciprocal_rom[53] = 26'h21c16df;
        reciprocal_rom[54] = 26'h21a58de;
        reciprocal_rom[55] = 26'h2189cdc;
        reciprocal_rom[56] = 26'h216e4db;
        reciprocal_rom[57] = 26'h2152ed9;
        reciprocal_rom[58] = 26'h2137cd8;
        reciprocal_rom[59] = 26'h211ccd7;
        reciprocal_rom[60] = 26'h2101ed5;
        reciprocal_rom[61] = 26'h20e74d4;
        reciprocal_rom[62] = 26'h20cccd2;
        reciprocal_rom[63] = 26'h20b28d2;
        reciprocal_rom[64] = 26'h20984d0;
        reciprocal_rom[65] = 26'h207e4ce;
        reciprocal_rom[66] = 26'h20648ce;
        reciprocal_rom[67] = 26'h204accc;
        reciprocal_rom[68] = 26'h20314cb;
        reciprocal_rom[69] = 26'h2017ec9;
        reciprocal_rom[70] = 26'h1ffecc9;
        reciprocal_rom[71] = 26'h1fe5ac7;
        reciprocal_rom[72] = 26'h1fcccc6;
        reciprocal_rom[73] = 26'h1fb40c4;
        reciprocal_rom[74] = 26'h1f9b8c4;
        reciprocal_rom[75] = 26'h1f830c2;
        reciprocal_rom[76] = 26'h1f6acc2;
        reciprocal_rom[77] = 26'h1f528c0;
        reciprocal_rom[78] = 26'h1f3a8bf;
        reciprocal_rom[79] = 26'h1f22abd;
        reciprocal_rom[80] = 26'h1f0b0bd;
        reciprocal_rom[81] = 26'h1ef36bc;
        reciprocal_rom[82] = 26'h1edbeba;
        reciprocal_rom[83] = 26'h1ec4ab9;
        reciprocal_rom[84] = 26'h1ead8b9;
        reciprocal_rom[85] = 26'h1e966b7;
        reciprocal_rom[86] = 26'h1e7f8b6;
        reciprocal_rom[87] = 26'h1e68cb5;
        reciprocal_rom[88] = 26'h1e522b4;
        reciprocal_rom[89] = 26'h1e3bab3;
        reciprocal_rom[90] = 26'h1e254b2;
        reciprocal_rom[91] = 26'h1e0f0b1;
        reciprocal_rom[92] = 26'h1df8eb0;
        reciprocal_rom[93] = 26'h1de2eae;
        reciprocal_rom[94] = 26'h1dcd2ae;
        reciprocal_rom[95] = 26'h1db76ad;
        reciprocal_rom[96] = 26'h1da1cac;
        reciprocal_rom[97] = 26'h1d8c4ab;
        reciprocal_rom[98] = 26'h1d76eaa;
        reciprocal_rom[99] = 26'h1d61aa9;
        reciprocal_rom[100] = 26'h1d4c8a8;
        reciprocal_rom[101] = 26'h1d378a7;
        reciprocal_rom[102] = 26'h1d22aa6;
        reciprocal_rom[103] = 26'h1d0dea6;
        reciprocal_rom[104] = 26'h1cf92a4;
        reciprocal_rom[105] = 26'h1ce4aa4;
        reciprocal_rom[106] = 26'h1cd02a2;
        reciprocal_rom[107] = 26'h1cbbea2;
        reciprocal_rom[108] = 26'h1ca7aa1;
        reciprocal_rom[109] = 26'h1c9389f;
        reciprocal_rom[110] = 26'h1c7faa0;
        reciprocal_rom[111] = 26'h1c6ba9e;
        reciprocal_rom[112] = 26'h1c57e9d;
        reciprocal_rom[113] = 26'h1c4449c;
        reciprocal_rom[114] = 26'h1c30c9c;
        reciprocal_rom[115] = 26'h1c1d49b;
        reciprocal_rom[116] = 26'h1c09e9a;
        reciprocal_rom[117] = 26'h1bf6a99;
        reciprocal_rom[118] = 26'h1be3898;
        reciprocal_rom[119] = 26'h1bd0898;
        reciprocal_rom[120] = 26'h1bbd896;
        reciprocal_rom[121] = 26'h1baac96;
        reciprocal_rom[122] = 26'h1b98095;
        reciprocal_rom[123] = 26'h1b85695;
        reciprocal_rom[124] = 26'h1b72c93;
        reciprocal_rom[125] = 26'h1b60693;
        reciprocal_rom[126] = 26'h1b4e092;
        reciprocal_rom[127] = 26'h1b3bc91;
        reciprocal_rom[128] = 26'h1b29a91;
        reciprocal_rom[129] = 26'h1b17890;
        reciprocal_rom[130] = 26'h1b0588f;
        reciprocal_rom[131] = 26'h1af3a8e;
        reciprocal_rom[132] = 26'h1ae1e8d;
        reciprocal_rom[133] = 26'h1ad048d;
        reciprocal_rom[134] = 26'h1abea8c;
        reciprocal_rom[135] = 26'h1aad28c;
        reciprocal_rom[136] = 26'h1a9ba8a;
        reciprocal_rom[137] = 26'h1a8a68a;
        reciprocal_rom[138] = 26'h1a79289;
        reciprocal_rom[139] = 26'h1a68089;
        reciprocal_rom[140] = 26'h1a56e88;
        reciprocal_rom[141] = 26'h1a45e87;
        reciprocal_rom[142] = 26'h1a35087;
        reciprocal_rom[143] = 26'h1a24285;
        reciprocal_rom[144] = 26'h1a13886;
        reciprocal_rom[145] = 26'h1a02c84;
        reciprocal_rom[146] = 26'h19f2484;
        reciprocal_rom[147] = 26'h19e1c83;
        reciprocal_rom[148] = 26'h19d1683;
        reciprocal_rom[149] = 26'h19c1082;
        reciprocal_rom[150] = 26'h19b0c81;
        reciprocal_rom[151] = 26'h19a0a81;
        reciprocal_rom[152] = 26'h1990880;
        reciprocal_rom[153] = 26'h198087f;
        reciprocal_rom[154] = 26'h1970a7f;
        reciprocal_rom[155] = 26'h1960c7e;
        reciprocal_rom[156] = 26'h195107d;
        reciprocal_rom[157] = 26'h194167d;
        reciprocal_rom[158] = 26'h1931c7d;
        reciprocal_rom[159] = 26'h192227b;
        reciprocal_rom[160] = 26'h1912c7c;
        reciprocal_rom[161] = 26'h190347a;
        reciprocal_rom[162] = 26'h18f407a;
        reciprocal_rom[163] = 26'h18e4c79;
        reciprocal_rom[164] = 26'h18d5a79;
        reciprocal_rom[165] = 26'h18c6879;
        reciprocal_rom[166] = 26'h18b7677;
        reciprocal_rom[167] = 26'h18a8877;
        reciprocal_rom[168] = 26'h1899a77;
        reciprocal_rom[169] = 26'h188ac76;
        reciprocal_rom[170] = 26'h187c075;
        reciprocal_rom[171] = 26'h186d675;
        reciprocal_rom[172] = 26'h185ec74;
        reciprocal_rom[173] = 26'h1850474;
        reciprocal_rom[174] = 26'h1841c74;
        reciprocal_rom[175] = 26'h1833472;
        reciprocal_rom[176] = 26'h1825072;
        reciprocal_rom[177] = 26'h1816c72;
        reciprocal_rom[178] = 26'h1808871;
        reciprocal_rom[179] = 26'h17fa671;
        reciprocal_rom[180] = 26'h17ec470;
        reciprocal_rom[181] = 26'h17de470;
        reciprocal_rom[182] = 26'h17d046f;
        reciprocal_rom[183] = 26'h17c266e;
        reciprocal_rom[184] = 26'h17b4a6e;
        reciprocal_rom[185] = 26'h17a6e6e;
        reciprocal_rom[186] = 26'h179926d;
        reciprocal_rom[187] = 26'h178b86d;
        reciprocal_rom[188] = 26'h177de6c;
        reciprocal_rom[189] = 26'h177066b;
        reciprocal_rom[190] = 26'h176306c;
        reciprocal_rom[191] = 26'h175586a;
        reciprocal_rom[192] = 26'h174846a;
        reciprocal_rom[193] = 26'h173b06a;
        reciprocal_rom[194] = 26'h172dc69;
        reciprocal_rom[195] = 26'h1720a69;
        reciprocal_rom[196] = 26'h1713869;
        reciprocal_rom[197] = 26'h1706667;
        reciprocal_rom[198] = 26'h16f9868;
        reciprocal_rom[199] = 26'h16ec867;
        reciprocal_rom[200] = 26'h16dfa66;
        reciprocal_rom[201] = 26'h16d2e66;
        reciprocal_rom[202] = 26'h16c6266;
        reciprocal_rom[203] = 26'h16b9665;
        reciprocal_rom[204] = 26'h16acc65;
        reciprocal_rom[205] = 26'h16a0264;
        reciprocal_rom[206] = 26'h1693a64;
        reciprocal_rom[207] = 26'h1687263;
        reciprocal_rom[208] = 26'h167ac63;
        reciprocal_rom[209] = 26'h166e663;
        reciprocal_rom[210] = 26'h1662062;
        reciprocal_rom[211] = 26'h1655c62;
        reciprocal_rom[212] = 26'h1649861;
        reciprocal_rom[213] = 26'h163d661;
        reciprocal_rom[214] = 26'h1631461;
        reciprocal_rom[215] = 26'h1625260;
        reciprocal_rom[216] = 26'h161925f;
        reciprocal_rom[217] = 26'h160d460;
        reciprocal_rom[218] = 26'h160145f;
        reciprocal_rom[219] = 26'h15f565e;
        reciprocal_rom[220] = 26'h15e9a5e;
        reciprocal_rom[221] = 26'h15dde5e;
        reciprocal_rom[222] = 26'h15d225d;
        reciprocal_rom[223] = 26'h15c685d;
        reciprocal_rom[224] = 26'h15bae5d;
        reciprocal_rom[225] = 26'h15af45c;
        reciprocal_rom[226] = 26'h15a3c5b;
        reciprocal_rom[227] = 26'h159865c;
        reciprocal_rom[228] = 26'h158ce5b;
        reciprocal_rom[229] = 26'h158185a;
        reciprocal_rom[230] = 26'h157645b;
        reciprocal_rom[231] = 26'h156ae5a;
        reciprocal_rom[232] = 26'h155fa59;
        reciprocal_rom[233] = 26'h1554859;
        reciprocal_rom[234] = 26'h1549659;
        reciprocal_rom[235] = 26'h153e458;
        reciprocal_rom[236] = 26'h1533459;
        reciprocal_rom[237] = 26'h1528257;
        reciprocal_rom[238] = 26'h151d458;
        reciprocal_rom[239] = 26'h1512457;
        reciprocal_rom[240] = 26'h1507656;
        reciprocal_rom[241] = 26'h14fca57;
        reciprocal_rom[242] = 26'h14f1c56;
        reciprocal_rom[243] = 26'h14e7055;
        reciprocal_rom[244] = 26'h14dc655;
        reciprocal_rom[245] = 26'h14d1c55;
        reciprocal_rom[246] = 26'h14c7255;
        reciprocal_rom[247] = 26'h14bc854;
        reciprocal_rom[248] = 26'h14b2054;
        reciprocal_rom[249] = 26'h14a7854;
        reciprocal_rom[250] = 26'h149d053;
        reciprocal_rom[251] = 26'h1492a53;
        reciprocal_rom[252] = 26'h1488453;
        reciprocal_rom[253] = 26'h147de52;
        reciprocal_rom[254] = 26'h1473a52;
        reciprocal_rom[255] = 26'h1469651;
    end
    reg [25:0] lookup = 0;
    reg signed [20:0] y1 = 0, y2 = 0;
    reg signed [31:0] angle1 = 0;
    reg signed [47:0] angle2 = 0;
    reg [1:0] scale1 = 0, scale2 = 0, scale3 = 0;
    reg zero1 = 1, zero2 = 1, zero3 = 1;
    reg valid1 = 0, valid2 = 0, valid3 = 0;
    // Stage 1: one synchronous ROM read alongside coordinate/angle metadata.
    always @(posedge clk) begin
        lookup <= reciprocal_rom[mantissa[18:11]];
        y1 <= y_in;
        angle1 <= angle_in + 32'sd128;
        scale1 <= scale;
        zero1 <= zero_in;
        valid1 <= resetn && valid_in;
    end
    // Stage 2 registers the complete interpolation in PREG. The distributed
    // lookup has a fast registered output; its fractional address is captured
    // in AREG at stage 1. PREG keeps the interpolation post-adder out of the
    // path to the next DSP multiplier, without adding a clock.
    wire signed [47:0] reciprocal_acc;
    phase_residual_mac #(.SUBTRACT(1), .REGISTER_RESULT(1), .REGISTER_A(1)) interpolation (
        .clk(clk), .a({19'd0,mantissa[10:0]}), .b({9'd0,lookup[8:0]}),
        .c({20'd0,lookup[25:9],1'b1,10'd0}), .p(reciprocal_acc)
    );
    always @(posedge clk) begin
        y2 <= y1;
        case (scale1)
            2: angle2 <= {{2{angle1[31]}},angle1,14'd0};
            1: angle2 <= {{3{angle1[31]}},angle1,13'd0};
            default: angle2 <= {{4{angle1[31]}},angle1,12'd0};
        endcase
        scale2 <= scale1;
        zero2 <= zero1;
        valid2 <= resetn && valid1;
    end
    wire signed [17:0] reciprocal = {1'b0,reciprocal_acc[27:11]};
    // Stage 3 registers the final product and aligned base angle inside
    // the DSP. Its post-adder and the phase slice are combinational outputs.
    wire signed [47:0] accumulated;
    phase_residual_mac final_angle (
        .clk(clk), .a({{9{y2[20]}},y2}), .b(reciprocal),
        .c(angle2), .p(accumulated)
    );
    always @(posedge clk) begin
        scale3 <= scale2;
        zero3 <= zero2;
        valid3 <= resetn && valid2;
    end
    wire signed [21:0] rounded = scale3 == 2 ? accumulated[43:22] :
                                 scale3 == 1 ? accumulated[42:21] : accumulated[41:20];
    assign phase_out = zero3 ? 24'sd0 : {{2{rounded[21]}},rounded};
    assign valid_out = valid3;
endmodule

// Signed MAC with explicit register boundaries. REGISTER_RESULT selects PREG
// instead of MREG/CREG; REGISTER_A captures its A operand one clock earlier.
module phase_residual_mac #(
    parameter integer SUBTRACT=0,
    parameter integer REGISTER_RESULT=0,
    parameter integer REGISTER_A=0
) (
    input wire clk,
    input wire signed [29:0] a,
    input wire signed [17:0] b,
    input wire signed [47:0] c,
    output wire signed [47:0] p
);
    DSP48E1 #(
        .AREG(REGISTER_A), .ACASCREG(REGISTER_A), .BREG(0), .BCASCREG(0),
        .MREG(REGISTER_RESULT ? 0 : 1), .CREG(REGISTER_RESULT ? 0 : 1),
        .PREG(REGISTER_RESULT ? 1 : 0), .ADREG(0), .DREG(0),
        .ALUMODEREG(0), .OPMODEREG(0), .INMODEREG(0),
        .CARRYINREG(0), .CARRYINSELREG(0), .USE_DPORT("FALSE"),
        .USE_MULT("MULTIPLY"), .USE_SIMD("ONE48")
    ) dsp (
        .CLK(clk), .A(a), .B(b), .C(c), .D(25'd0), .P(p),
        .OPMODE(7'b0110101), .ALUMODE(SUBTRACT ? 4'b0011 : 4'b0000),
        .INMODE(5'd0), .CARRYIN(1'b0), .CARRYINSEL(3'd0),
        .ACIN(30'd0), .BCIN(18'd0), .PCIN(48'd0),
        .CARRYCASCIN(1'b0), .MULTSIGNIN(1'b0),
        .CEA1(1'b1), .CEA2(1'b1), .CEB1(1'b1), .CEB2(1'b1),
        .CEAD(1'b1), .CEALUMODE(1'b1), .CEC(1'b1), .CECARRYIN(1'b1),
        .CECTRL(1'b1), .CED(1'b1), .CEINMODE(1'b1), .CEM(1'b1), .CEP(1'b1),
        .RSTA(1'b0), .RSTB(1'b0), .RSTC(1'b0), .RSTD(1'b0),
        .RSTALUMODE(1'b0), .RSTCTRL(1'b0), .RSTINMODE(1'b0),
        .RSTALLCARRYIN(1'b0), .RSTM(1'b0), .RSTP(1'b0)
    );
endmodule
