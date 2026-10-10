`timescale 1 ns / 1 ps
module manual_p_corrector #(
    parameter integer FUSED=1,
    parameter integer GAIN_STAGES=2,
    parameter integer FAST_GAIN_STAGES=GAIN_STAGES,
    parameter integer FAST_P_DSP=0,
    parameter integer PIPELINED_REFERENCE=0,
    parameter integer PRECOMBINE_I=0,
    parameter integer TAIL_GAIN_STAGES=3,
    parameter integer I2_GAIN_STAGES=TAIL_GAIN_STAGES,
    parameter integer FINAL_CSA_LEVELS=2,
    parameter integer CARRY_BLOCK=0,
    parameter integer SELECTOR_CARRY_BLOCK=CARRY_BLOCK,
    parameter integer FREQ_WIDTH=17,
    parameter integer PHASE_WIDTH=32,
    parameter integer PHASE_FRAC=0,
    parameter integer PI_REGISTER_ADDRESS=0
)(
    input wire clk, resetn,
    input wire signed [FREQ_WIDTH-1:0] freq_in,
    input wire signed [PHASE_WIDTH-1:0] phase_in,
    input wire signed [15:0] i_in, q_in,
    input wire signed [31:0] cx, cy,
    input wire request_fast, snapshot_request,
    input wire [2:0] enabled,
    input wire [3:0] active_banks,
    input wire [8:0] table_command,
    input wire [63:0] table_data,
    output wire [15:0] fast_corr, slow_corr,
    output wire [31:0] snapshot, path_status
);
    wire signed [16:0] fast_phase,fast_phase_pre;
    wire active_fast, in_range, snapshot_ack;
    wire [31:0] accurate, integral, accurate_i, higher, fast_p, fast_pi, selected;
    reg [31:0] fast_i_acc=0;
    wire signed [PHASE_WIDTH-1:0] reference_phase;
    wire signed [PHASE_WIDTH-1:0] phase_delta=$signed(fast_phase);
    reg signed [PHASE_WIDTH-1:0] fast_i_phase=0;
    wire [PHASE_WIDTH-1:0] phase_delta_shifted=phase_delta<<<PHASE_FRAC;
    generate if(PIPELINED_REFERENCE) begin : reference_pipeline
        reg [16:0] low=0;
        reg [PHASE_WIDTH-17:0] high_zero=0,high_one=0;
        wire [PHASE_WIDTH-17:0] high_zero_next,high_one_next;
        dpll_carry_adder #(.WIDTH(PHASE_WIDTH-16),.BLOCK(8)) high_add_zero(
            reference_phase[PHASE_WIDTH-1:16],phase_delta_shifted[PHASE_WIDTH-1:16],1'b0,high_zero_next);
        dpll_carry_adder #(.WIDTH(PHASE_WIDTH-16),.BLOCK(8)) high_add_one(
            reference_phase[PHASE_WIDTH-1:16],phase_delta_shifted[PHASE_WIDTH-1:16],1'b1,high_one_next);
        always @(posedge clk) begin
            low<={1'b0,reference_phase[15:0]}+{1'b0,phase_delta_shifted[15:0]};
            high_zero<=high_zero_next;
            high_one<=high_one_next;
            fast_i_phase<={low[16] ? high_one : high_zero,low[15:0]};
        end
    end else begin : reference_single
        always @(posedge clk) fast_i_phase<=reference_phase+phase_delta_shifted;
    end endgenerate
    wire [31:0] fast_i_next;
    dpll_carry_adder #(.BLOCK(8)) fast_i_add(fast_i_acc,fast_pi,1'b0,fast_i_next);
    always @(posedge clk) begin
        if(!enabled[1]) fast_i_acc<=0;
        else if(!active_fast) fast_i_acc<=accurate_i;
        else fast_i_acc<=fast_i_next;
    end


    table_corrector #(.FUSED(FUSED),.GAIN_STAGES(GAIN_STAGES),
        .TAIL_GAIN_STAGES(TAIL_GAIN_STAGES),.I2_GAIN_STAGES(I2_GAIN_STAGES),.FINAL_CSA_LEVELS(FINAL_CSA_LEVELS),
        .CARRY_BLOCK(CARRY_BLOCK),.FREQ_WIDTH(FREQ_WIDTH),
        .PHASE_WIDTH(PHASE_WIDTH),.PHASE_FRAC(PHASE_FRAC),.PI_REGISTER_ADDRESS(PI_REGISTER_ADDRESS)) accurate_controller(
        .clk(clk),.freq_in(freq_in),.phase_in(phase_in),.enabled(enabled),
        .active_banks(active_banks),.table_command(table_command),.table_data(table_data),
        .fast_corr(),.slow_corr(slow_corr),.correction(accurate),.p_correction(),.integral_correction(integral),
        .i_correction(accurate_i),.higher_correction(higher));
    fast_p_detector detector(clk,i_in,q_in,cx[24:0],cy[24:0],fast_phase,in_range,fast_phase_pre);
    p_reference_capture #(.PHASE_WIDTH(PHASE_WIDTH)) capture(
        clk,resetn,snapshot_request,i_in,q_in,snapshot_ack,snapshot,phase_in,reference_phase);
    // Program and commit the extra P table with the same transactions/bank as
    // the accurate P gain. No additional software gain programming is needed.
    table_gain #(.A_WIDTH(17),.OUTPUT_WIDTH(32),.PIPE_STAGES(FAST_GAIN_STAGES),.DSP_FAST_P(FAST_P_DSP),
        .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS),.CARRY_BLOCK(CARRY_BLOCK)) fast_gain(
        clk,(FAST_P_DSP && FAST_GAIN_STAGES==3) ? fast_phase_pre : fast_phase,active_banks[0],table_command[8] && table_command[1:0]==0,
        table_command[7],table_command[6],table_command[5:2],table_data,fast_p);
    table_gain #(.A_WIDTH(PHASE_WIDTH),.OUTPUT_LOW(16+PHASE_FRAC),.OUTPUT_WIDTH(32),
        .PIPE_STAGES(GAIN_STAGES),.FINAL_CSA_LEVELS(FINAL_CSA_LEVELS),.CARRY_BLOCK(CARRY_BLOCK)) fast_i_gain(
        clk,fast_i_phase,active_banks[1],table_command[8] && table_command[1:0]==1,
        table_command[7],table_command[6],table_command[5:2],table_data,fast_pi);
    p_path_switch #(.CARRY_BLOCK(SELECTOR_CARRY_BLOCK),.PRECOMBINE_I(PRECOMBINE_I)) selector(clk,resetn,enabled[1],request_fast,
        accurate,higher,fast_p,selected,active_fast,fast_i_acc);
    assign fast_corr=selected[31:16];
    // [0] effective mode, [1] fast detector range, [2] capture acknowledgement.
    assign path_status={12'b0,fast_phase, snapshot_ack,in_range,active_fast};
endmodule
