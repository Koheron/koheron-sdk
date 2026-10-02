`timescale 1ns/1ps
module test_phase_prefilter;
  reg clk=0;
  always #2.5 clk=~clk;
  reg reset_n=0;
  reg signed [15:0] din=0;
  wire signed [15:0] dout;
  wire signed [23:0] wide_dout;
  wire [63:0] randoms [0:13];
  wire [13:0] valid;
  function automatic [63:0] seed(input integer channel);
    case(channel)
      0: seed=64'h9e3779b97f4a7c15;
      1: seed=64'hd1b54a32d192ed03;
      2: seed=64'h94d049bb133111eb;
      3: seed=64'h8538ecb5bd456ea3;
      4: seed=64'h2545f4914f6cdd1d;
      5: seed=64'h6a09e667f3bcc909;
      6: seed=64'h9e3779b97f4a7c15 ^ 64'h5bd1e9956c8e9cf5;
      7: seed=64'h9e3779b97f4a7c15 ^ 64'h27d4eb2f165667c5;
      8: seed=64'hd1b54a32d192ed03 ^ 64'h5bd1e9956c8e9cf5;
      9: seed=64'hd1b54a32d192ed03 ^ 64'h27d4eb2f165667c5;
      10: seed=64'h94d049bb133111eb ^ 64'h5bd1e9956c8e9cf5;
      11: seed=64'h94d049bb133111eb ^ 64'h27d4eb2f165667c5;
      12: seed=64'h8538ecb5bd456ea3 ^ 64'h5bd1e9956c8e9cf5;
      13: seed=64'h8538ecb5bd456ea3 ^ 64'h27d4eb2f165667c5;
    endcase
  endfunction
  genvar g;
  generate for(g=0;g<14;g=g+1) begin : rng
    axis_lfsr #(.SEED(seed(g)),.FEEDBACK_MASK(64'hd800000000000000),
      .FEEDBACK_XNOR(0)) r(.aclk(clk),.aresetn(reset_n),
      .m_axis_tready(1'b1),.m_axis_tdata(randoms[g]),.m_axis_tvalid(valid[g]));
  end endgenerate
  phase_prefilter filter(clk,reset_n,din,randoms[0][31:16],dout);
  phase_prefilter #(.OUTPUT_WIDTH(24)) wide_filter(clk,reset_n,din,randoms[0][31:16],wide_dout);
  integer coefficients[0:60], history[0:64], ones[0:13], correlations[0:90];
  integer i,j,a,b,c,d,pair_index, expected, wide_expected, rounding;
  longint signed sum;
  initial begin
    for(i=0;i<141;i=i+1) coefficients[i]=0;
    // Independent direct convolution of four rectangular kernels.
    for(a=0;a<16;a=a+1) for(b=0;b<16;b=b+1)
      for(c=0;c<16;c=c+1) for(d=0;d<16;d=d+1)
        coefficients[a+b+c+d]=coefficients[a+b+c+d]+1;
    for(i=0;i<145;i=i+1) history[i]=0;
    for(i=0;i<14;i=i+1) ones[i]=0;
    for(i=0;i<91;i=i+1) correlations[i]=0;
    for(a=0;a<14;a=a+1) for(b=a+1;b<14;b=b+1)
      if(seed(a)==seed(b) || seed(a)==0 || seed(b)==0) $fatal(1,"Duplicate or zero rounding seed");
    repeat(4) @(negedge clk);
    reset_n=1;
    for(i=0;i<100000;i=i+1) begin
      @(negedge clk);
      // Steady extrema, impulses, cancellation, fractional values and noise.
      if(i<100) din=32767;
      else if(i<200) din=-32768;
      else if(i<400) din=(i==250 ? 32767 : 0);
      else if(i<800) din=(i%2 ? -32768 : 32767);
      else if(i<1200) din=(i%2 ? -1 : 0);
      else din=$random;
      for(j=64;j>0;j=j-1) history[j]=history[j-1];
      history[0]=din;
      sum=0;
      for(j=0;j<61;j=j+1) sum=sum+longint'(history[j+4])*coefficients[j];
      rounding=randoms[0][31:16];
      expected=(sum+rounding)>>>16;
      wide_expected=(sum+(rounding&255))>>>8;
      for(a=0;a<14;a=a+1) begin
        if(randoms[a]==0) $fatal(1,"LFSR entered absorbing zero state");
        ones[a]=ones[a]+randoms[a][0];
      end
      pair_index=0;
      for(a=0;a<14;a=a+1) for(b=a+1;b<14;b=b+1) begin
        correlations[pair_index]=correlations[pair_index]+
          (randoms[a][0]==randoms[b][0] ? 1 : -1);
        pair_index=pair_index+1;
      end
      @(posedge clk); #1;
      if(dout!==expected) $fatal(1,"Prefilter convolution/rounding mismatch at %d: %d %d",i,dout,expected);
      if(wide_dout!==wide_expected) $fatal(1,"Wide prefilter mismatch at %d: %d %d",i,wide_dout,wide_expected);
    end
    for(a=0;a<14;a=a+1)
      if(ones[a]<48000 || ones[a]>52000) $fatal(1,"LFSR balance: channel %d ones %d",a,ones[a]);
    for(a=0;a<91;a=a+1)
      if(correlations[a]<-2000 || correlations[a]>2000) $fatal(1,"LFSR correlation %d",correlations[a]);
    $display("LFSR ones: %d %d %d %d",ones[0],ones[1],ones[2],ones[3]);
    $display("LFSR pair signed sums: %d %d %d %d %d %d",correlations[0],correlations[1],correlations[2],correlations[3],correlations[4],correlations[5]);
    @(negedge clk); reset_n=0;
    @(posedge clk); #1;
    if(dout!==0 || wide_dout!==0) $fatal(1,"Prefilter reset failed");
    for(a=0;a<14;a=a+1)
      if(randoms[a]!==seed(a)) $fatal(1,"LFSR reset seed failed");
    $display("Prefilter checks passed: exact full-precision convolution, signed extremes, stochastic rounding and reset");
    $display("Rounding generators passed: 14 distinct seeds, balance and all 91 pair correlations over 100000 clocks");
    $finish;
  end
endmodule
