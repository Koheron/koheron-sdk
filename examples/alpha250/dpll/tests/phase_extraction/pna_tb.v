`timescale 1 ns / 1 ps
module pna_phase_compare;
reg clk=0;
reg signed [23:0] i_in=0,q_in=0;
reg valid=0;
wire vc,vv;
wire signed [23:0] pc;
wire [47:0] pv;
phase_extractor custom(clk,1'b1,valid,i_in,q_in,vc,pc);
system_cordic_0 vendor(.aclk(clk),.s_axis_cartesian_tvalid(valid),.s_axis_cartesian_tdata({q_in,i_in}),.m_axis_dout_tvalid(vv),.m_axis_dout_tdata(pv));
always #2 clk=~clk;
integer xi[0:27],yi[0:27];
real ref_phase[0:27];
reg [27:0] vpipe=0;
integer k,src,dst,status,dx,dy,ev,vb,rb,cycles=0;
real exact=0;
always @(posedge clk) begin
  for(k=27;k>0;k=k-1) begin xi[k]=xi[k-1];yi[k]=yi[k-1];ref_phase[k]=ref_phase[k-1];end
  xi[0]=i_in;yi[0]=q_in;ref_phase[0]=exact;
  vpipe={vpipe[26:0],valid};
  cycles=cycles+1;
  #0.1;
  if(vc !== vpipe[13]) $fatal(1,"Custom latency mismatch cycle %d",cycles);
  if(vv !== vpipe[27]) $fatal(1,"Vendor latency mismatch cycle %d",cycles);
  if(vc) $fwrite(dst,"C %d %d %d %.12f\n",xi[13],yi[13],$signed(pc),ref_phase[13]);
  if(vv) $fwrite(dst,"V %d %d %d %.12f\n",xi[27],yi[27],$signed(pv[47:24]),ref_phase[27]);
end
initial begin
  for(k=0;k<28;k=k+1) begin xi[k]=0;yi[k]=0;ref_phase[k]=0;end
  src=$fopen("vectors.txt","r");dst=$fopen("results.txt","w");
  if(!src || !dst) $fatal(1,"File open failed");
  repeat(30) @(negedge clk);
  status=6;
  while(status==6) begin
    status=$fscanf(src,"%d %d %d %d %d %f\n",dx,dy,ev,vb,rb,exact);
    if(status==6) begin i_in=dx;q_in=dy;valid=1;@(negedge clk);end
  end
  if(status != -1) $fatal(1,"Malformed vector file");
  valid=0;
  repeat(32) @(negedge clk);
  $fclose(dst);
  $display("Direct comparison passed: custom 14 clocks, PNA 28 clocks");
  $finish;
end
endmodule
