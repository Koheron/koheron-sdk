`timescale 1ns/1ps
module test_mixer_product;
  reg clk=0;
  always #2.5 clk=~clk;
  reg reset_n=0, input_valid=0;
  reg signed [15:0] ar=0,ai=0,br=0,bi=0;
  wire valid;
  wire [79:0] product;
  reg [16:0] random_i=0,random_q=0;
  wire signed [15:0] rounded_i,rounded_q;
  system_complex_mult_0 cmpy(clk,input_valid,{ai,ar},input_valid,{bi,br},valid,product);
  phase_round #(.INPUT_WIDTH(33),.SHIFT(17),.OUTPUT_WIDTH(16)) ri(clk,reset_n,product[32:0],random_i,rounded_i);
  phase_round #(.INPUT_WIDTH(33),.SHIFT(17),.OUTPUT_WIDTH(16)) rq(clk,reset_n,product[72:40],random_q,rounded_q);
  longint signed expected_i[0:1007],expected_q[0:1007],round_i,round_q;
  integer sent=0,received=0,checked=0,i;
  reg check_round;
  always @(posedge clk) begin
    check_round=valid && reset_n;
    round_i=(longint'($signed(product[32:0]))+longint'(random_i))>>>17;
    round_q=(longint'($signed(product[72:40]))+longint'(random_q))>>>17;
    #1;
    if(check_round) begin
      if(longint'(rounded_i)!==round_i || longint'(rounded_q)!==round_q) $fatal(1,"Signed mixer shift or component slice mismatch");
      checked=checked+1;
    end
    if(valid) begin
      if(received>=sent) $fatal(1,"Unexpected output");
      if(longint'($signed(product[39:0]))!==expected_i[received] || longint'($signed(product[79:40]))!==expected_q[received])
        $fatal(1,"Full-product or byte packing mismatch at %d: %d %d expected %d %d",received,$signed(product[39:0]),$signed(product[79:40]),expected_i[received],expected_q[received]);
      received=received+1;
    end
  end
  initial begin
    repeat(8) @(negedge clk);
    reset_n=1;
    for(i=0;i<1008;i=i+1) begin
      @(negedge clk);
      input_valid=1;
      if(i<8) begin
        ar=(i&1 ? -32768 : 32767); ai=(i&2 ? -32768 : 32767);
        br=(i&4 ? -32768 : 32767); bi=-32768;
      end else begin ar=$random;ai=$random;br=$random;bi=$random; end
      random_i=$random;random_q=$random;
      expected_i[sent]=longint'(ar)*longint'(br)-longint'(ai)*longint'(bi);
      expected_q[sent]=longint'(ar)*longint'(bi)+longint'(ai)*longint'(br);
      sent=sent+1;
    end
    @(negedge clk);input_valid=0;
    repeat(16) @(negedge clk);
    if(received!=sent || checked!=sent) $fatal(1,"Pipeline lost a sample: sent %d product %d rounded %d",sent,received,checked);
    $display("Vendor mixer checks passed: 1008 full complex products, 40-bit component packing and signed 17-bit stochastic conversion");
    $finish;
  end
endmodule
