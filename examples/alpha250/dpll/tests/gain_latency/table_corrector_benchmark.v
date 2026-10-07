`timescale 1 ns / 1 ps
module table_corrector_benchmark #(
    parameter integer FUSED=1,
    parameter integer GAIN_STAGES=3,
    parameter integer TAIL_GAIN_STAGES=GAIN_STAGES,
    parameter integer FINAL_CSA_LEVELS=0,
    parameter integer CARRY_BLOCK=0
)(
    input wire clk,
    input wire [16:0] freq,
    input wire [31:0] phase,
    input wire [2:0] enabled,
    input wire [3:0] active_banks,
    input wire [8:0] table_command,
    input wire [63:0] table_data,
    output reg [31:0] result=0
);
    reg [16:0] freq_reg=0;
    reg [31:0] phase_reg=0;
    reg [2:0] enabled_reg=0;
    reg [3:0] banks_reg=0;
    reg [8:0] command_reg=0;
    reg [63:0] data_reg=0;
    wire [15:0] fast,slow;
    always @(posedge clk) begin
        freq_reg<=freq;phase_reg<=phase;enabled_reg<=enabled;
        banks_reg<=active_banks;command_reg<=table_command;data_reg<=table_data;
        result<={slow,fast};
    end
    table_corrector #(.FUSED(FUSED), .GAIN_STAGES(GAIN_STAGES), .TAIL_GAIN_STAGES(TAIL_GAIN_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK)) corrector(clk,freq_reg,phase_reg,enabled_reg,banks_reg,command_reg,data_reg,fast,slow,,,);
endmodule
