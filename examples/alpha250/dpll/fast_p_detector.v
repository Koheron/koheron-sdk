`timescale 1 ns / 1 ps
// Calibrated quadrature projection around a captured lock reference.
// Coefficients are signed Q6.18, including 8192/pi phase units per radian.
// Calibration requires A0>=64, hence |coefficient|<=8192/(pi*64)<64.
// With reference (I0,Q0): cx=-Q0*S/(I0^2+Q0^2), cy=I0*S/(I0^2+Q0^2).
// phase = S*(A/A0)*sin(phi-phi0). No atan, division or unwrap in this path.
module fast_p_detector(
    input wire clk,
    input wire signed [15:0] i_in, q_in,
    input wire signed [24:0] cx, cy,
    output reg signed [16:0] phase=0,
    output reg in_range=0,
    output wire signed [16:0] phase_pre
);
    // A 16x25 product fits one DSP. Register its inputs ahead of the products,
    // separating external routing from the multiplier's setup requirement.
    // The calibration bound retains all 18 fractional bits with no truncation.
    reg signed [15:0] i_d=0,q_d=0;
    reg signed [24:0] cx_d=0,cy_d=0;
    reg signed [40:0] ix=0, qy=0, iy=0, qx=0;
    reg signed [41:0] quadrature=0, radial=0;
    always @(posedge clk) begin
        i_d<=i_in; q_d<=q_in; cx_d<=cx; cy_d<=cy;
        ix<=i_d*cx_d; qy<=q_d*cy_d;
        iy<=i_d*cy_d; qx<=q_d*cx_d;
        quadrature<={ix[40],ix}+{qy[40],qy};
        radial<={iy[40],iy}-{qx[40],qx};
    end
    wire signed [23:0] projected=quadrature[41:18];
    // I and status use registered clipping. Fast P captures phase_pre directly
    // in its DSP input register on the same clock, avoiding an extra stage.
    wire clipped=|(projected[22:16] ^ {7{projected[23]}});
    assign phase_pre={projected[23],clipped ? {16{!projected[23]}} : projected[15:0]};
    always @(posedge clk) phase<=phase_pre;
    // Diagnostic only: no automatic mode changes. Approximately +/-14 degrees,
    // with radial amplitude at least half the calibration amplitude.
    // Range reporting is outside the P feedback path. Register its absolute
    // value and comparison separately from the AXI status read mux.
    reg [41:0] magnitude_bits=0, abs_q=0;
    reg negative_q=0;
    reg signed [41:0] radial_first=0;
    reg signed [41:0] radial_d=0;
    always @(posedge clk) begin
        magnitude_bits<=quadrature ^ {42{quadrature[41]}};
        negative_q<=quadrature[41];
        abs_q<=magnitude_bits+negative_q;
        radial_first<=radial; radial_d<=radial_first;
        in_range<=radial_d>42'sd341782528 && {abs_q,2'b00} < {2'b00,radial_d};
    end
endmodule

module p_reference_capture #(parameter integer PHASE_WIDTH=32)(
    input wire clk, resetn,
    input wire request,
    input wire signed [15:0] i_in, q_in,
    output reg ack=0,
    output reg [31:0] snapshot=0,
    input wire signed [PHASE_WIDTH-1:0] phase_in,
    output reg signed [PHASE_WIDTH-1:0] reference_phase=0
);
    reg busy=0, pending=0;
    reg [6:0] count=0;
    reg signed [21:0] sum_i=0, sum_q=0;
    reg signed [PHASE_WIDTH-1:0] origin=0;
    reg signed [PHASE_WIDTH+5:0] phase_sum=0;
    reg signed [PHASE_WIDTH-1:0] delta_d=0;
    wire signed [21:0] next_i=sum_i+{{6{i_in[15]}},i_in};
    wire signed [21:0] next_q=sum_q+{{6{q_in[15]}},q_in};
    always @(posedge clk) begin
        if(!resetn) begin
            busy<=0; ack<=0; snapshot<=0;
            count<=0; sum_i<=0; sum_q<=0; pending<=0;
            phase_sum<=0; reference_phase<=0; origin<=0; delta_d<=0;
        end else if(!busy && request!=ack) begin
            pending<=request; busy<=1;
            count<=0; sum_i<=0; sum_q<=0;
            phase_sum<=0; delta_d<=0;
        end else if(busy && count<64) begin
            if(count==0) origin<=phase_in;
            delta_d<=count==0 ? 0 : phase_in-origin;
            if(count!=0) phase_sum<=phase_sum+delta_d;
            sum_i<=next_i; sum_q<=next_q;
            if(count==63) begin
                snapshot<={next_q[21:6],next_i[21:6]};
            end
            count<=count+1'b1;
        end else if(busy && count==64) begin
            // Drain the last registered difference before averaging.
            phase_sum<=phase_sum+delta_d;
            count<=65;
        end else if(busy) begin
            reference_phase<=origin+(phase_sum>>>6);
            ack<=pending; busy<=0;
        end
    end
endmodule
