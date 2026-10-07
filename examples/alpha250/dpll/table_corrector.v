`timescale 1 ns / 1 ps
// Production accurate P/I/I3 gains use four stages, I2 uses five; DSP Fast P
// takes three clocks from its earlier projection tap. Defaults retain the
// historical three-clock controller.
module table_corrector #(
    parameter integer FUSED=1,
    parameter integer GAIN_STAGES=3,
    parameter integer TAIL_GAIN_STAGES=GAIN_STAGES,
    parameter integer I2_GAIN_STAGES=TAIL_GAIN_STAGES,
    parameter integer FINAL_CSA_LEVELS=0,
    parameter integer CARRY_BLOCK=0,
    parameter integer FREQ_WIDTH=17,
    parameter integer PHASE_WIDTH=32,
    parameter integer PHASE_FRAC=0
)(
    input wire clk,
    input wire signed [FREQ_WIDTH-1:0] freq_in,
    input wire signed [PHASE_WIDTH-1:0] phase_in,
    input wire [2:0] enabled,
    input wire [3:0] active_banks,
    input wire [8:0] table_command,
    input wire [63:0] table_data,
    output wire [15:0] fast_corr,
    output wire [15:0] slow_corr,
    output wire [31:0] correction,
    output wire [31:0] p_correction,
    output wire [31:0] integral_correction,
    output wire [31:0] i_correction,
    output wire [31:0] higher_correction
);
    wire [3:0] we;
    genvar k;
    generate for(k=0;k<4;k=k+1) begin : decode
        assign we[k]=table_command[8] && table_command[1:0]==k;
    end endgenerate
    wire [31:0] p,pi,i2;
    wire [63:0] i3;
    reg [31:0] first_sum=0;
    reg [31:0] first_p=0;
    reg [31:0] first_pi=0;
    reg [31:0] p_acc=0;
    reg [31:0] integral_acc=0;
    reg [31:0] i_acc=0;
    reg [31:0] higher_acc=0;
    reg signed [47:0] acc1=0;
    (* use_dsp="yes" *) reg [31:0] acc2=0;
    reg [63:0] acc3=0;
    table_gain #(.A_WIDTH(FREQ_WIDTH), .OUTPUT_LOW(PHASE_FRAC), .OUTPUT_WIDTH(32), .PIPE_STAGES(GAIN_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gp(clk,freq_in,active_banks[0],we[0],table_command[7],table_command[6],table_command[5:2],table_data,p);
    table_gain #(.A_WIDTH(PHASE_WIDTH), .OUTPUT_LOW(16+PHASE_FRAC), .OUTPUT_WIDTH(32), .PIPE_STAGES(GAIN_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gpi(clk,phase_in,active_banks[1],we[1],table_command[7],table_command[6],table_command[5:2],table_data,pi);
    table_gain #(.A_WIDTH(48), .OUTPUT_LOW(48), .OUTPUT_WIDTH(32), .PIPE_STAGES(I2_GAIN_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gi2(clk,acc1,active_banks[2],we[2],table_command[7],table_command[6],table_command[5:2],table_data,i2);
    table_gain #(.A_WIDTH(32), .OUTPUT_WIDTH(64), .PIPE_STAGES(TAIL_GAIN_STAGES), .FINAL_CSA_LEVELS(FINAL_CSA_LEVELS), .CARRY_BLOCK(CARRY_BLOCK))
        gi3(clk,acc2,active_banks[3],we[3],table_command[7],table_command[6],table_command[5:2],table_data,i3);
    wire [31:0] first_next;
    wire [47:0] acc1_next;
    wire [63:0] acc3_next;
    dpll_carry_adder #(.BLOCK(CARRY_BLOCK)) first_add(p,pi,1'b0,first_next);
    dpll_carry_adder #(.WIDTH(48),.BLOCK(CARRY_BLOCK)) acc1_add(
        acc1,{{16{first_sum[31]}},first_sum},1'b0,acc1_next);
    dpll_carry_adder #(.WIDTH(64),.BLOCK(CARRY_BLOCK)) acc3_add(acc3,i3,1'b0,acc3_next);
    always @(posedge clk) begin
        first_sum<=first_next;
        first_p<=p;
        first_pi<=pi;
        if(!enabled[0]) acc1<=0;
        else acc1<=acc1_next;
        if(!enabled[2]) acc3<=0;
        else acc3<=acc3_next;
    end
    generate if(FUSED) begin : fused
        // Combine the second summing node and fast accumulator in one clock.
        // The native case maps the three-input accumulation into one DSP.
        // Other states and the optional carry-block implementation retain a
        // carry-save compressor followed by one carry-propagating adder.
        wire [31:0] sum=acc2^first_sum^i2;
        wire [31:0] carry=((acc2&first_sum)|(acc2&i2)|(first_sum&i2))<<1;
        wire [31:0] integral_sum=integral_acc^first_pi^i2;
        wire [31:0] integral_carry=((integral_acc&first_pi)|(integral_acc&i2)|(first_pi&i2))<<1;
        wire [31:0] acc2_next,integral_next,p_next,i_next,higher_next;
        dpll_carry_adder #(.BLOCK(CARRY_BLOCK)) acc2_add(sum,carry,1'b0,acc2_next);
        dpll_carry_adder #(.BLOCK(CARRY_BLOCK)) integral_add(integral_sum,integral_carry,1'b0,integral_next);
        dpll_carry_adder #(.BLOCK(CARRY_BLOCK)) p_add(p_acc,first_p,1'b0,p_next);
        dpll_carry_adder #(.BLOCK(CARRY_BLOCK)) i_add(i_acc,first_pi,1'b0,i_next);
        dpll_carry_adder #(.BLOCK(CARRY_BLOCK)) higher_add(higher_acc,i2,1'b0,higher_next);
        always @(posedge clk) begin
            if(!enabled[1]) acc2<=0;
            else if(CARRY_BLOCK==0) acc2<=acc2+first_sum+i2;
            else acc2<=acc2_next;
            if(!enabled[1]) p_acc<=0;
            else p_acc<=p_next;
            if(!enabled[1]) integral_acc<=0;
            else integral_acc<=integral_next;
            if(!enabled[1]) begin i_acc<=0; higher_acc<=0; end
            else begin i_acc<=i_next; higher_acc<=higher_next; end
        end
    end else begin : separate
        reg [31:0] second_sum=0;
        reg [31:0] second_p=0;
        reg [31:0] second_integral=0;
        reg [31:0] second_i=0,second_higher=0;
        always @(posedge clk) begin
            second_sum<=first_sum+i2;
            second_p<=first_p;
            second_integral<=first_pi+i2;
            second_i<=first_pi; second_higher<=i2;
            if(!enabled[1]) acc2<=0;
            else acc2<=acc2+second_sum;
            if(!enabled[1]) p_acc<=0;
            else p_acc<=p_acc+second_p;
            if(!enabled[1]) integral_acc<=0;
            else integral_acc<=integral_acc+second_integral;
            if(!enabled[1]) begin i_acc<=0; higher_acc<=0; end
            else begin i_acc<=i_acc+second_i; higher_acc<=higher_acc+second_higher; end
        end
    end endgenerate
    assign fast_corr=acc2[31:16];
    assign slow_corr={~acc3[63],acc3[62:48]};
    // Track only the direct accumulated P term. All higher-order branches
    // continue using the original accurate detector and original state.
    assign correction=acc2;
    assign p_correction=p_acc;
    // acc2 == p_acc + integral_acc modulo 2^32. Keeping this state directly
    // removes subtraction of the accurate P term from the fast output path.
    assign integral_correction=integral_acc;
    assign i_correction=i_acc;
    assign higher_correction=higher_acc;
endmodule
