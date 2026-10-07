`timescale 1 ns / 1 ps
// Both sources run continuously. Switch complete corrections, never phase
// increments. All arithmetic is modulo 2^32, matching the original controller.
module p_path_switch #(parameter integer CARRY_BLOCK=0, parameter integer PRECOMBINE_I=0)(
    input wire clk, resetn, enabled, request_fast,
    input wire [31:0] accurate, integral, fast_p,
    output reg [31:0] correction=0,
    output reg active_fast=0,
    input wire [31:0] fast_i
);
    reg [31:0] offset=0;
    reg [3:0] warmup=0;
    wire [31:0] integral_source;
    generate if(PRECOMBINE_I) begin : premix
        reg [31:0] value=0;
        always @(posedge clk) value<=integral+fast_i;
        assign integral_source=value;
    end else assign integral_source=integral; endgenerate
    wire [31:0] final_sum,final_carry;
    generate if(PRECOMBINE_I) begin : short_sum
        assign final_sum=integral_source^fast_p^offset;
        assign final_carry=((integral_source&fast_p)|(integral_source&offset)|(fast_p&offset))<<1;
    end else begin : full_sum
        wire [31:0] s=integral^fast_p^fast_i;
        wire [31:0] c=((integral&fast_p)|(integral&fast_i)|(fast_p&fast_i))<<1;
        assign final_sum=s^c^offset;
        assign final_carry=((s&c)|(s&offset)|(c&offset))<<1;
    end endgenerate
    // Compress the multioperand correction before one carry-propagating add.
    // Straight a+b-c+offset expressions create cascaded carry chains at 250 MHz.
    wire [31:0] n2,c2;
    generate if(PRECOMBINE_I) begin : short_offset
        wire [31:0] n=correction^~integral_source^~fast_p;
        wire [31:0] c=((correction&~integral_source)|(correction&~fast_p)|(~integral_source&~fast_p))<<1;
        assign n2=n^c^32'd2;
        assign c2=((n&c)|(n&32'd2)|(c&32'd2))<<1;
    end else begin : full_offset
        wire [31:0] n0=correction^~integral^~fast_p;
        wire [31:0] c0=((correction&~integral)|(correction&~fast_p)|(~integral&~fast_p))<<1;
        wire [31:0] n1=n0^c0^~fast_i;
        wire [31:0] c1=((n0&c0)|(n0&~fast_i)|(c0&~fast_i))<<1;
        assign n2=n1^c1^32'd3;
        assign c2=((n1&c1)|(n1&32'd3)|(c1&32'd3))<<1;
    end endgenerate
    wire [31:0] fast_next,accurate_next,fast_offset,accurate_offset;
    dpll_carry_adder #(.BLOCK(CARRY_BLOCK)) fast_add(final_sum,final_carry,1'b0,fast_next);
    dpll_carry_adder #(.BLOCK(CARRY_BLOCK)) accurate_add(accurate,offset,1'b0,accurate_next);
    dpll_carry_adder #(.BLOCK(CARRY_BLOCK)) handoff_fast(n2,c2,1'b0,fast_offset);
    dpll_carry_adder #(.BLOCK(CARRY_BLOCK)) handoff_accurate(correction,~accurate,1'b1,accurate_offset);
    always @(posedge clk) begin
        if(!resetn || !enabled) begin
            correction<=0; offset<=0; active_fast<=0; warmup<=0;
        end else if(request_fast!=active_fast) begin
            // Hold exactly one output sample and anchor the new source to it.
            // Offsets persist through a return to Accurate; integrator disable
            // or peripheral reset clears them. No integral state is reset here.
            if(!request_fast || warmup==(PRECOMBINE_I ? 15 : 7)) begin
                offset<=request_fast ? fast_offset : accurate_offset;
                active_fast<=request_fast; warmup<=0;
            end else begin
                // All calibration registers precede the mode request. Allow
                // the projection and gain pipelines to fill before handoff.
                correction<=accurate_next; warmup<=warmup+1'b1;
            end
        end else begin
            correction<=active_fast ? fast_next : accurate_next;
            warmup<=0;
        end
    end
endmodule
