`timescale 1 ns / 1 ps

module phase_unwrapper #
(
  parameter integer DIN_WIDTH = 16,
  parameter integer DOUT_WIDTH = 32,
  parameter integer PIPELINED_OVERFLOW = 0,
  parameter integer PIPELINED_HISTORY = 0
)
(
  input  wire clk,
  input  wire acc_on,
  input  wire rst,
  input  wire signed [DIN_WIDTH-1:0] phase_in,
  output wire signed [DIN_WIDTH+1-1:0] freq_out,
  output reg signed [DOUT_WIDTH-1:0] phase_out,
  output reg overflow
);

  // Value of Pi in scaled radians representation
  localparam PI = 2**(DIN_WIDTH-3);
  localparam TWOPI = 2**(DIN_WIDTH-2);

  reg signed [DIN_WIDTH-1:0] phase_in0;
  reg signed [DIN_WIDTH+1-1:0] diff;
  reg signed [DIN_WIDTH+1-1:0] unwrapped_diff;

  initial phase_out = 0;
  initial unwrapped_diff = 0;
  initial phase_in0 = 0;
  initial diff = 0;
  initial overflow = 0;

  wire signed [DOUT_WIDTH:0] next_phase;
  phase_unwrapper_adder #(.WIDTH(DOUT_WIDTH+1),.BLOCK(PIPELINED_OVERFLOW ? 8 : 0)) history_add(
      {phase_out[DOUT_WIDTH-1],phase_out},
      {{(DOUT_WIDTH-DIN_WIDTH){unwrapped_diff[DIN_WIDTH]}},unwrapped_diff},
      1'b0,next_phase);

  // Compute phase difference
  always @(posedge clk) begin
    phase_in0 <= phase_in;
    diff <= phase_in - phase_in0;
  end

  // Pi and 2*pi are powers of two. A wrap changes only the three high
  // bits; explicit reductions also retain the strict comparisons at +/-pi.
  generate if (DIN_WIDTH >= 4) begin : bit_wrap
    wire above_pi = !diff[DIN_WIDTH] &&
        ((|diff[DIN_WIDTH-1:DIN_WIDTH-2]) ||
         (diff[DIN_WIDTH-3] && (|diff[DIN_WIDTH-4:0])));
    wire below_pi = diff[DIN_WIDTH] && !(&diff[DIN_WIDTH-1:DIN_WIDTH-3]);
    wire [2:0] upper = diff[DIN_WIDTH:DIN_WIDTH-2];
    wire [2:0] upper_minus = upper - 3'd1;
    wire [2:0] upper_plus = upper + 3'd1;
    always @(posedge clk)
      unwrapped_diff <= {above_pi ? upper_minus : below_pi ? upper_plus : upper,
                         diff[DIN_WIDTH-3:0]};
  end else begin : narrow_wrap
  always @(posedge clk) begin
    if (diff > PI) begin
      unwrapped_diff <= diff - TWOPI;
    end else if (diff < -PI) begin
      unwrapped_diff <= diff + TWOPI;
    end else begin
      unwrapped_diff <= diff;
    end
  end
  end endgenerate

  // Accumulate phase. The monitor can delay its history one clock while
  // keeping the frequency bus and independent feedback accumulator unchanged.
  generate if (PIPELINED_HISTORY) begin : split_history
    if (DOUT_WIDTH <= 32 || DIN_WIDTH >= 32 || !PIPELINED_OVERFLOW)
      initial $error("Split history requires a wide monitor and delayed overflow");
    reg [31:0] low_state=0;
    reg signed [1:0] upper_step=0;
    reg overflow_pending=0;
    wire [31:0] increment_low={{(31-DIN_WIDTH){unwrapped_diff[DIN_WIDTH]}},unwrapped_diff};
    wire [32:0] low_next={1'b0,low_state}+{1'b0,increment_low};
    wire signed [DOUT_WIDTH-32:0] upper_next=
        $signed({phase_out[DOUT_WIDTH-1],phase_out[DOUT_WIDTH-1:32]})+$signed(upper_step);
    always @(posedge clk) begin
      if(rst) begin
        low_state<=0;upper_step<=0;phase_out<=0;
        overflow_pending<=0;overflow<=0;
      end else begin
        if(acc_on) begin
          low_state<=low_next[31:0];
          upper_step<=unwrapped_diff[DIN_WIDTH] ?
              (low_next[32] ? 2'sd0 : -2'sd1) : (low_next[32] ? 2'sd1 : 2'sd0);
        end else upper_step<=0;
        phase_out<={upper_next[DOUT_WIDTH-33:0],low_state};
        overflow_pending<=upper_next[DOUT_WIDTH-32]!=upper_next[DOUT_WIDTH-33];
        overflow<=overflow | overflow_pending;
      end
    end
  end else begin : full_history
  always @(posedge clk) begin
    if (rst) begin
      phase_out <= 0;
    end else begin
      if (acc_on) begin
        phase_out <= next_phase[DOUT_WIDTH-1:0];
      end else begin
        phase_out <= phase_out;
      end
    end
  end
  end endgenerate

  generate if (!PIPELINED_HISTORY && PIPELINED_OVERFLOW) begin : delayed_overflow
    // Check signs after the sum has been registered. Only the sticky error
    // flag is delayed; phase and frequency samples retain their latency.
    // PNA filtering delays the affected sample well beyond this one clock.
    reg previous_sign=0, difference_sign=0, accumulated=0;
    always @(posedge clk) begin
      if (rst) begin
        previous_sign <= 0;
        difference_sign <= 0;
        accumulated <= 0;
        overflow <= 0;
      end else begin
        previous_sign <= phase_out[DOUT_WIDTH-1];
        difference_sign <= unwrapped_diff[DIN_WIDTH];
        accumulated <= acc_on;
        overflow <= overflow | (accumulated && previous_sign==difference_sign &&
                               phase_out[DOUT_WIDTH-1]!=previous_sign);
      end
    end
  end else if (!PIPELINED_HISTORY) begin : immediate_overflow
    always @(posedge clk) begin
      if (rst) overflow <= 0;
      else if (acc_on) overflow <= overflow | (next_phase[DOUT_WIDTH] != next_phase[DOUT_WIDTH-1]);
    end
  end endgenerate

  assign freq_out = unwrapped_diff;

endmodule

// Short local carry chains, connected by the 7-series dedicated carry fabric.
// Selecting this implementation adds no registers or sample delay.
module phase_unwrapper_adder #(
    parameter integer WIDTH=32,
    parameter integer BLOCK=0
)(input wire [WIDTH-1:0] x,y, input wire cin, output wire [WIDTH-1:0] sum);
    generate if(BLOCK==0) begin : ripple
        assign sum=x+y+cin;
    end else begin : bounded
        localparam BLOCKS=(WIDTH+BLOCK-1)/BLOCK;
        localparam GROUPS=(BLOCKS+3)/4;
        wire [GROUPS*4-1:0] g,p;
        wire [GROUPS*4:0] c;
        assign c[0]=cin;
        genvar i;
        for(i=0;i<BLOCKS;i=i+1) begin : chunk
            localparam N=(WIDTH-i*BLOCK<BLOCK) ? WIDTH-i*BLOCK : BLOCK;
            wire [N-1:0] a=x[i*BLOCK +: N],b=y[i*BLOCK +: N];
            // Read the terminal CO directly. Inferring an N+1-bit sum adds
            // another carry cell just to produce the block's carry-out.
            localparam LOCAL_GROUPS=(N+3)/4;
            wire [LOCAL_GROUPS*4-1:0] local_p,local_di,zero,one;
            wire [LOCAL_GROUPS*4:0] zero_c,one_c;
            assign zero_c[0]=0; assign one_c[0]=1;
            assign local_p={{(LOCAL_GROUPS*4-N){1'b1}},a^b};
            assign local_di={{(LOCAL_GROUPS*4-N){1'b0}},a};
            genvar j;
            for(j=0;j<LOCAL_GROUPS;j=j+1) begin : local_group
                CARRY4 carry_zero(.CI(j==0 ? 1'b0 : zero_c[4*j]),.CYINIT(1'b0),
                    .DI(local_di[4*j +: 4]),.S(local_p[4*j +: 4]),
                    .CO(zero_c[4*j+1 +: 4]),.O(zero[4*j +: 4]));
                CARRY4 carry_one(.CI(j==0 ? 1'b0 : one_c[4*j]),.CYINIT(j==0 ? 1'b1 : 1'b0),
                    .DI(local_di[4*j +: 4]),.S(local_p[4*j +: 4]),
                    .CO(one_c[4*j+1 +: 4]),.O(one[4*j +: 4]));
            end
            assign g[i]=zero_c[N]; assign p[i]=&(a^b);
            assign sum[i*BLOCK +: N]=c[i] ? one[N-1:0] : zero[N-1:0];
        end
        for(i=BLOCKS;i<GROUPS*4;i=i+1) begin : pad
            assign g[i]=0; assign p[i]=0;
        end
        for(i=0;i<GROUPS;i=i+1) begin : carry_group
            CARRY4 chain(.CI(i==0 ? 1'b0 : c[4*i]),.CYINIT(i==0 ? cin : 1'b0),.DI(g[4*i +: 4]),.S(p[4*i +: 4]),
                         .CO(c[4*i+1 +: 4]),.O());
        end
    end endgenerate
endmodule
