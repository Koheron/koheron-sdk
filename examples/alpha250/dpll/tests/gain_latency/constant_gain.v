`timescale 1 ns / 1 ps

// Constant mantissa bank. Fixed shifts and carry-save adders implement the
// twelve coefficients. Both polarities are computed before the selection
// stage, avoiding a variable negation carry chain after the octave selector.
module constant_gain #(
    parameter integer A_WIDTH = 32,
    parameter integer OUTPUT_LOW = 0,
    parameter integer OUTPUT_WIDTH = 64,
    parameter integer PIPE_STAGES = 2,
    parameter integer FRACTION_BITS = 11
)(
    input wire CLK,
    input wire signed [A_WIDTH-1:0] A,
    input wire [4:0] SELECT,
    input wire [4:0] OCTAVE,
    output reg signed [OUTPUT_WIDTH-1:0] P = 0
);
    localparam WIDTH = A_WIDTH + FRACTION_BITS + 2;
    localparam SHIFT_WIDTH = WIDTH + 31;
    function integer coefficient(input integer index);
        if (FRACTION_BITS == 7) begin
        case (index)
            0: coefficient=128; 1: coefficient=136; 2: coefficient=144;
            3: coefficient=152; 4: coefficient=162; 5: coefficient=171;
            6: coefficient=181; 7: coefficient=192; 8: coefficient=204;
            9: coefficient=216; 10: coefficient=228; 11: coefficient=242;
            default: coefficient=0;
        endcase
        end else begin
        case (index)
            0: coefficient=2048; 1: coefficient=2170; 2: coefficient=2299;
            3: coefficient=2435; 4: coefficient=2580; 5: coefficient=2734;
            6: coefficient=2896; 7: coefficient=3069; 8: coefficient=3251;
            9: coefficient=3444; 10: coefficient=3649; 11: coefficient=3866;
            default: coefficient=0;
        endcase
        end
    endfunction
    wire signed [WIDTH-1:0] a_ext = A;
    wire [WIDTH-1:0] products [0:23];
    reg [4:0] select1 = 31, octave1 = 0, select2 = 31, octave2 = 0;
    always @(posedge CLK) begin
        select1 <= SELECT; octave1 <= OCTAVE;
        select2 <= select1; octave2 <= octave1;
    end
    genvar bank;
    generate for (bank=0; bank<24; bank=bank+1) begin : constants
        localparam VALUE = (bank<12) ? coefficient(bank) : -coefficient(bank-12);
        constant_product #(.A_WIDTH(A_WIDTH), .WIDTH(WIDTH), .VALUE(VALUE),
                           .PIPE_STAGES(PIPE_STAGES-1))
            multiply(CLK, A, products[bank]);
    end endgenerate
    wire [4:0] selected = (PIPE_STAGES == 2) ? select1 : select2;
    wire [4:0] octave = (PIPE_STAGES == 2) ? octave1 : octave2;
    reg signed [WIDTH-1:0] product;
    always @* begin
        case (selected)
            0: product=products[0]; 1: product=products[1];
            2: product=products[2]; 3: product=products[3];
            4: product=products[4]; 5: product=products[5];
            6: product=products[6]; 7: product=products[7];
            8: product=products[8]; 9: product=products[9];
            10: product=products[10]; 11: product=products[11];
            12: product=products[12]; 13: product=products[13];
            14: product=products[14]; 15: product=products[15];
            16: product=products[16]; 17: product=products[17];
            18: product=products[18]; 19: product=products[19];
            20: product=products[20]; 21: product=products[21];
            22: product=products[22]; 23: product=products[23];
            default: product=0;
        endcase
    end
    wire signed [SHIFT_WIDTH-1:0] extended = product;
    wire signed [SHIFT_WIDTH-1:0] scaled = extended <<< octave;
    always @(posedge CLK) P <= scaled >>> (FRACTION_BITS+OUTPUT_LOW);
endmodule

// Compile-time canonical signed digits, followed by a balanced carry-save
// tree and one carry-propagating sum. No DSPs or runtime coefficient multiply.
module constant_product #(
    parameter integer A_WIDTH = 32,
    parameter integer WIDTH = A_WIDTH+13,
    parameter integer VALUE = 2048,
    parameter integer PIPE_STAGES = 1
)(input wire CLK, input wire signed [A_WIDTH-1:0] A,
  output reg [WIDTH-1:0] P = 0);
    function integer digit(input integer index);
        integer n, k, d;
        begin
            n = (VALUE<0) ? -VALUE : VALUE;
            digit=0;
            for (k=0; k<=12; k=k+1) begin
                d = (n%2) ? 2-(n%4) : 0;
                if (k==index) digit = (VALUE<0) ? -d : d;
                n=(n-d)/2;
            end
        end
    endfunction
    function integer count(input integer negative_only);
        integer k;
        begin
            count=0;
            for (k=0; k<=12; k=k+1)
                if (negative_only ? digit(k)<0 : digit(k)!=0) count=count+1;
        end
    endfunction
    function integer position(input integer index);
        integer k, n;
        begin
            n=0; position=0;
            for (k=0; k<=12; k=k+1) begin
                if (digit(k)!=0) begin
                    if (n==index) position=k;
                    n=n+1;
                end
            end
        end
    endfunction
    localparam TERMS=count(0)+1;
    function integer term_count(input integer level);
        integer k;
        begin
            term_count=TERMS;
            for (k=0; k<level; k=k+1)
                term_count=2*(term_count/3)+term_count%3;
        end
    endfunction
    function integer tree_depth(input integer n);
        begin
            tree_depth=0;
            while(n>2) begin n=2*(n/3)+n%3; tree_depth=tree_depth+1; end
        end
    endfunction
    localparam LEVELS=tree_depth(TERMS);
    wire [WIDTH-1:0] tree [0:(LEVELS+1)*TERMS-1];
    wire signed [WIDTH-1:0] extended=A;
    genvar i, l, g, r;
    generate for(i=0; i<TERMS-1; i=i+1) begin : term
        localparam POS=position(i);
        wire [WIDTH-1:0] shifted=extended <<< POS;
        assign tree[i]=(digit(POS)<0) ? ~shifted : shifted;
    end endgenerate
    assign tree[TERMS-1]=count(1);
    generate for(l=0; l<LEVELS; l=l+1) begin : level
        localparam N=term_count(l);
        for(g=0; g<N/3; g=g+1) begin : triple
            wire [WIDTH-1:0] x=tree[l*TERMS+3*g];
            wire [WIDTH-1:0] y=tree[l*TERMS+3*g+1];
            wire [WIDTH-1:0] z=tree[l*TERMS+3*g+2];
            assign tree[(l+1)*TERMS+2*g]=x^y^z;
            assign tree[(l+1)*TERMS+2*g+1]=((x&y)|(x&z)|(y&z))<<1;
        end
        for(r=0; r<N%3; r=r+1) begin : remainder
            assign tree[(l+1)*TERMS+2*(N/3)+r]=tree[l*TERMS+3*(N/3)+r];
        end
    end endgenerate
    generate if(PIPE_STAGES==2) begin : pipeline
        reg [WIDTH-1:0] sum=0, carry=0;
        always @(posedge CLK) begin
            sum<=tree[LEVELS*TERMS]; carry<=tree[LEVELS*TERMS+1];
            P<=sum+carry;
        end
    end else begin
        always @(posedge CLK) P<=tree[LEVELS*TERMS]+tree[LEVELS*TERMS+1];
    end endgenerate
endmodule
