`timescale 1 ns / 1 ps
// Selected 250 MHz configuration: FUSED=1, GAIN_STAGES=2, TAIL_GAIN_STAGES=3,
// FINAL_CSA_LEVELS=2, CARRY_BLOCK=0. Defaults retain the three-clock fallback.
module table_corrector #(
    parameter integer FUSED=1,
    parameter integer GAIN_STAGES=3,
    parameter integer TAIL_GAIN_STAGES=GAIN_STAGES,
    parameter integer FINAL_CSA_LEVELS=0,
    parameter integer CARRY_BLOCK=0
)(
    input wire clk,
    input wire signed [16:0] freq_in,
    input wire signed [31:0] phase_in,
    input wire [2:0] enabled,
    input wire [3:0] active_banks,
    input wire [8:0] table_command,
    input wire [63:0] table_data,
    output wire [15:0] fast_corr,
    output wire [15:0] slow_corr
);
    wire [3:0] we;
    genvar k;
    generate for(k=0;k<4;k=k+1) begin : decode
        assign we[k]=table_command[8] && table_command[1:0]==k;
    end endgenerate
    wire [31:0] p,pi,i2;
    wire [63:0] i3;
    reg [31:0] first_sum=0;
    reg signed [47:0] acc1=0;
    reg [31:0] acc2=0;
    reg [63:0] acc3=0;
    table_gain #(.A_WIDTH(17), .OUTPUT_WIDTH(32), .PIPE_STAGES(GAIN_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gp(clk,freq_in,active_banks[0],we[0],table_command[7],table_command[6],table_command[5:2],table_data,p);
    table_gain #(.A_WIDTH(32), .OUTPUT_LOW(16), .OUTPUT_WIDTH(32), .PIPE_STAGES(GAIN_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gpi(clk,phase_in,active_banks[1],we[1],table_command[7],table_command[6],table_command[5:2],table_data,pi);
    table_gain #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32), .PIPE_STAGES(TAIL_GAIN_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gi2(clk,acc1,active_banks[2],we[2],table_command[7],table_command[6],table_command[5:2],table_data,i2);
    table_gain #(.A_WIDTH(32), .OUTPUT_WIDTH(64), .PIPE_STAGES(TAIL_GAIN_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gi3(clk,acc2,active_banks[3],we[3],table_command[7],table_command[6],table_command[5:2],table_data,i3);
    always @(posedge clk) begin
        first_sum<=p+pi;
        if(!enabled[0]) acc1<=0;
        else acc1<=acc1+$signed(first_sum);
        if(!enabled[2]) acc3<=0;
        else acc3<=acc3+i3;
    end
    generate if(FUSED) begin : fused
        // Combine the second summing node and fast accumulator in one clock.
        // A carry-save compressor followed by one 32-bit carry chain avoids
        // cascading two carry-propagating adders.
        wire [31:0] sum=acc2^first_sum^i2;
        wire [31:0] carry=((acc2&first_sum)|(acc2&i2)|(first_sum&i2))<<1;
        always @(posedge clk) begin
            if(!enabled[1]) acc2<=0;
            else acc2<=sum+carry;
        end
    end else begin : separate
        reg [31:0] second_sum=0;
        always @(posedge clk) begin
            second_sum<=first_sum+i2;
            if(!enabled[1]) acc2<=0;
            else acc2<=acc2+second_sum;
        end
    end endgenerate
    assign fast_corr=acc2[31:16];
    assign slow_corr={~acc3[63],acc3[62:48]};
endmodule
