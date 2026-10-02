// One phase value per clock. The sine conversion is a separate DDS LUT.
// X is a 48-bit phase increment with 16 fractional guard bits.
// C = round((r^16 - 1) * 2^64); X[n+16] = X[n] + round(X[n]*C/2^64).
// Sixteen interleaved states leave time to pipeline the wide multiplication.
module chirp_generator (
    input wire clk,
    input wire aresetn,
    input wire reset,
    input wire trigger,
    input wire [31:0] sample_count,
    input wire [47:0] coefficient,
    input wire [63:0] seed_data,
    input wire [3:0] seed_index,
    input wire seed_write,
    output reg [47:0] phase,
    output reg phase_valid
);
    reg [63:0] state [0:15];
    reg active, trigger_d, seed_write_d;
    reg [31:0] count;
    reg [3:0] lane;
    wire [63:0] current_state = state[lane];
    wire [47:0] increment = current_state[63:16] + current_state[15];

    // Stage 0: read state; stages 1-2: nine DSP-sized partial products.
    reg [63:0] x [0:8];
    reg [3:0] tag [0:8];
    reg [8:0] valid;
    wire [71:0] extended_x = {8'b0, x[0]};
    wire [50:0] extended_c = {3'b0, coefficient};
    wire [111:0] term [0:8];
    genvar a, b;
    generate for (a=0; a<3; a=a+1) begin: row
        for (b=0; b<3; b=b+1) begin: col
            (* use_dsp = "yes" *) reg signed [42:0] product;
            reg [40:0] product_d;
            always @(posedge clk) begin
                product <= $signed({1'b0, extended_x[24*a +: 24]}) *
                           $signed({1'b0, extended_c[17*b +: 17]});
                product_d <= product[40:0];
            end
            assign term[3*a+b] = {71'b0, product_d} << (24*a+17*b);
        end
    end endgenerate

    // Stages 3-6: balanced adder tree. Stage 7: round high product bits.
    reg [111:0] sum3 [0:4];
    reg [111:0] sum4 [0:2];
    reg [111:0] sum5 [0:1];
    reg [111:0] sum6;
    reg [48:0] delta7;
    // Stages 8-9: split the state addition to avoid a 64-bit carry path.
    reg [32:0] low8;
    reg [31:0] high8;
    wire [63:0] next_state = {high8 + {31'b0, low8[32]}, low8[31:0]};
    integer i;
    always @(posedge clk) begin
        x[0] <= current_state;
        tag[0] <= lane;
        for (i=1; i<9; i=i+1) begin
            x[i] <= x[i-1];
            tag[i] <= tag[i-1];
        end
        for (i=0; i<4; i=i+1) sum3[i] <= term[2*i] + term[2*i+1];
        sum3[4] <= term[8];
        sum4[0] <= sum3[0] + sum3[1];
        sum4[1] <= sum3[2] + sum3[3];
        sum4[2] <= sum3[4];
        sum5[0] <= sum4[0] + sum4[1];
        sum5[1] <= sum4[2];
        sum6 <= sum5[0] + sum5[1];
        delta7 <= {1'b0, sum6[111:64]} + sum6[63];
        low8 <= {1'b0, x[7][31:0]} + {1'b0, delta7[31:0]};
        high8 <= x[7][63:32] + {15'b0, delta7[48:32]};

        trigger_d <= trigger;
        seed_write_d <= seed_write;
        phase_valid <= active;
        valid <= {valid[7:0], active};
        if (seed_write && !seed_write_d && !active)
            state[seed_index] <= seed_data;
        else if (valid[8] && !reset && aresetn)
            state[tag[8]] <= next_state;

        if (!aresetn || reset) begin
            active <= 0;
            trigger_d <= 0;
            seed_write_d <= 0;
            count <= 0;
            lane <= 0;
            phase <= 0;
            phase_valid <= 0;
            valid <= 0;
        end else if (trigger && !trigger_d && !active) begin
            active <= 1;
            count <= 0;
            lane <= 0;
            phase <= 0;
        end else if (active) begin
            phase <= phase + increment;
            lane <= lane + 1'b1;
            count <= count + 1'b1;
            if (count == sample_count-1) active <= 0;
        end
    end
endmodule
