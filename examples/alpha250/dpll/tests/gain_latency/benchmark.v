`timescale 1 ns / 1 ps
// Identical registered boundaries around all four gains of one DPLL.
// MODE=0: production, MODE=2/3: geometric two/three-cycle MREG prototype,
// MODE=4/5: geometric two/three-cycle PREG prototype.
// MODE=6/7: same with unsigned A chunks and sign correction in the DSP adder.
// MODE=8..11: constant product banks. MODE=12..19: programmable product tables.
// MODE=20/21: tables with balanced two-stage reduction, ripple/carry-select sum.
module gain_benchmark #(
    parameter integer MODE = 0
)(
    input wire clk,
    input wire [128:0] samples,
    input wire [127:0] coefficients,
    input wire [19:0] octaves,
    input wire [12:0] table_command,
    input wire [63:0] table_data,
    input wire [3:0] active_banks,
    output reg [159:0] result = 0
);
    // Separate signal registers match the four independent corrector inputs.
    reg [128:0] samples_reg = 0;
    reg [127:0] coefficients_reg = 0;
    reg [19:0] octaves_reg = 0;
    reg [12:0] table_command_reg=0;
    reg [63:0] table_data_reg=0;
    reg [3:0] active_banks_reg=0;
    always @(posedge clk) begin
        samples_reg <= samples;
        coefficients_reg <= coefficients;
        octaves_reg <= octaves;
        table_command_reg<=table_command;
        table_data_reg<=table_data;
        active_banks_reg<=active_banks;
    end
    wire [31:0] p, pi, i2;
    wire [63:0] i3;
    localparam STAGES = (MODE >= 4) ? 2+(MODE%2) : MODE;
    localparam DSP_OUTPUT_REG = (MODE >= 4) ? 1 : 0;
    localparam DSP_SIGN_CORRECTION = (MODE >= 6) ? 1 : 0;
    generate if (MODE == 0) begin
        gain_multiplier #(.A_WIDTH(17), .OUTPUT_WIDTH(32))
            gp(clk, samples_reg[16:0], coefficients_reg[31:0], p);
        gain_multiplier #(.A_WIDTH(32), .OUTPUT_LOW(16), .OUTPUT_WIDTH(32))
            gpi(clk, samples_reg[48:17], coefficients_reg[63:32], pi);
        gain_multiplier #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32))
            gi2(clk, samples_reg[96:49], coefficients_reg[95:64], i2);
        gain_multiplier #(.A_WIDTH(32), .OUTPUT_WIDTH(64))
            gi3(clk, samples_reg[128:97], coefficients_reg[127:96], i3);
    end else if (MODE >= 12 && MODE <= 21) begin
        localparam CBITS=(MODE==18 || MODE>=20) ? 4 : ((MODE==19) ? 6 : ((MODE>=16) ? 8 : ((MODE==12) ? 4 : ((MODE==13) ? 5 : 6))));
        localparam FRAC=(MODE==15 || MODE==17) ? 7 : 11;
        localparam PIPE=(MODE==18 || MODE==19) ? 3 : 2;
        wire [3:0] we;
        for(genvar k=0;k<4;k=k+1) begin : write_decode
            assign we[k]=table_command_reg[12] && table_command_reg[1:0]==k;
        end
        table_gain #(.A_WIDTH(17), .OUTPUT_WIDTH(32), .CHUNK_BITS(CBITS), .FRACTION_BITS(FRAC), .PIPE_STAGES(PIPE), .FINAL_CSA_LEVELS((MODE>=20) ? 2 : 0), .CARRY_BLOCK((MODE==21) ? 24 : 0))
            gp(clk,samples_reg[16:0],active_banks_reg[0],we[0],table_command_reg[11],table_command_reg[10],table_command_reg[CBITS+1:2],table_data_reg,p);
        table_gain #(.A_WIDTH(32), .OUTPUT_LOW(16), .OUTPUT_WIDTH(32), .CHUNK_BITS(CBITS), .FRACTION_BITS(FRAC), .PIPE_STAGES(PIPE), .FINAL_CSA_LEVELS((MODE>=20) ? 2 : 0), .CARRY_BLOCK((MODE==21) ? 24 : 0))
            gpi(clk,samples_reg[48:17],active_banks_reg[1],we[1],table_command_reg[11],table_command_reg[10],table_command_reg[CBITS+1:2],table_data_reg,pi);
        table_gain #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32), .CHUNK_BITS(CBITS), .FRACTION_BITS(FRAC), .PIPE_STAGES(PIPE), .FINAL_CSA_LEVELS((MODE>=20) ? 2 : 0), .CARRY_BLOCK((MODE==21) ? 24 : 0))
            gi2(clk,samples_reg[96:49],active_banks_reg[2],we[2],table_command_reg[11],table_command_reg[10],table_command_reg[CBITS+1:2],table_data_reg,i2);
        table_gain #(.A_WIDTH(32), .OUTPUT_WIDTH(64), .CHUNK_BITS(CBITS), .FRACTION_BITS(FRAC), .PIPE_STAGES(PIPE), .FINAL_CSA_LEVELS((MODE>=20) ? 2 : 0), .CARRY_BLOCK((MODE==21) ? 24 : 0))
            gi3(clk,samples_reg[128:97],active_banks_reg[3],we[3],table_command_reg[11],table_command_reg[10],table_command_reg[CBITS+1:2],table_data_reg,i3);
    end else if (MODE >= 8 && MODE <= 11) begin
        localparam FRAC=(MODE>=10) ? 7 : 11;
        constant_gain #(.A_WIDTH(17), .OUTPUT_WIDTH(32), .PIPE_STAGES(2+MODE%2), .FRACTION_BITS(FRAC))
            gp(clk, samples_reg[16:0], coefficients_reg[4:0], octaves_reg[4:0], p);
        constant_gain #(.A_WIDTH(32), .OUTPUT_LOW(16), .OUTPUT_WIDTH(32), .PIPE_STAGES(2+MODE%2), .FRACTION_BITS(FRAC))
            gpi(clk, samples_reg[48:17], coefficients_reg[36:32], octaves_reg[9:5], pi);
        constant_gain #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32), .PIPE_STAGES(2+MODE%2), .FRACTION_BITS(FRAC))
            gi2(clk, samples_reg[96:49], coefficients_reg[68:64], octaves_reg[14:10], i2);
        constant_gain #(.A_WIDTH(32), .OUTPUT_WIDTH(64), .PIPE_STAGES(2+MODE%2), .FRACTION_BITS(FRAC))
            gi3(clk, samples_reg[128:97], coefficients_reg[100:96], octaves_reg[19:15], i3);
    end else begin
        geometric_gain #(.A_WIDTH(17), .OUTPUT_WIDTH(32), .PIPE_STAGES(STAGES), .DSP_OUTPUT_REG(DSP_OUTPUT_REG), .DSP_SIGN_CORRECTION(DSP_SIGN_CORRECTION))
            gp(clk, samples_reg[16:0], coefficients_reg[12:0], coefficients_reg[25:13], octaves_reg[4:0], p);
        geometric_gain #(.A_WIDTH(32), .OUTPUT_LOW(16), .OUTPUT_WIDTH(32), .PIPE_STAGES(STAGES), .DSP_OUTPUT_REG(DSP_OUTPUT_REG), .DSP_SIGN_CORRECTION(DSP_SIGN_CORRECTION))
            gpi(clk, samples_reg[48:17], coefficients_reg[44:32], coefficients_reg[57:45], octaves_reg[9:5], pi);
        geometric_gain #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32), .PIPE_STAGES(STAGES), .DSP_OUTPUT_REG(DSP_OUTPUT_REG), .DSP_SIGN_CORRECTION(DSP_SIGN_CORRECTION))
            gi2(clk, samples_reg[96:49], coefficients_reg[76:64], coefficients_reg[89:77], octaves_reg[14:10], i2);
        geometric_gain #(.A_WIDTH(32), .OUTPUT_WIDTH(64), .PIPE_STAGES(STAGES), .DSP_OUTPUT_REG(DSP_OUTPUT_REG), .DSP_SIGN_CORRECTION(DSP_SIGN_CORRECTION))
            gi3(clk, samples_reg[128:97], coefficients_reg[108:96], coefficients_reg[121:109], octaves_reg[19:15], i3);
    end endgenerate
    always @(posedge clk) result <= {i3, i2, pi, p};
endmodule
