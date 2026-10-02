`timescale 1ns/1ps
module test_fpga_blocks;
 reg clk=0;always #2.5 clk=~clk;
 reg reset_n=0, phase_reset=1, accumulate=1;
 reg signed [15:0] phase_in=0, data_in=0;
 wire signed [31:0] phase_out;
 wire signed [16:0] frequency;
 wire signed [39:0] wide_phase_out;
 wire signed [24:0] wide_frequency;
 wire signed [23:0] wide_phase_in={phase_in,8'b0};
 wire signed [15:0] data_out;
 wire [63:0] random0,random1,random2,random3;
 wire valid0,valid1,valid2,valid3;
 phase_unwrapper unwrap(clk,accumulate,phase_reset,phase_in,frequency,phase_out);
 phase_unwrapper #(.DIN_WIDTH(24),.DOUT_WIDTH(40)) wide_unwrap(
   clk,accumulate,phase_reset,wide_phase_in,wide_frequency,wide_phase_out);
 boxcar_filter boxcar(clk,data_in,data_out);
 axis_lfsr r0(.aclk(clk),.aresetn(reset_n),.m_axis_tready(1'b1),.m_axis_tdata(random0),.m_axis_tvalid(valid0));
 axis_lfsr r1(.aclk(clk),.aresetn(reset_n),.m_axis_tready(1'b1),.m_axis_tdata(random1),.m_axis_tvalid(valid1));
 axis_lfsr r2(.aclk(clk),.aresetn(reset_n),.m_axis_tready(1'b1),.m_axis_tdata(random2),.m_axis_tvalid(valid2));
 axis_lfsr r3(.aclk(clk),.aresetn(reset_n),.m_axis_tready(1'b1),.m_axis_tdata(random3),.m_axis_tvalid(valid3));
 integer history[0:5];integer i,j,expected,total,ramp,wrapped,ones;
 initial begin
  for(j=0;j<6;j=j+1)history[j]=0;
  repeat(8)begin @(negedge clk);data_in=0;@(posedge clk);#1;end
  @(negedge clk);reset_n=1;phase_reset=0;
  ones=0;
  for(i=0;i<20000;i=i+1)begin
   @(negedge clk);
   ramp=i<10000 ? i*13 : 9999*13-(i-9999)*13;
   wrapped=((ramp+8192)%16384+16384)%16384-8192;
   phase_in=wrapped;
   case(i%5)
    0:data_in=-32768;1:data_in=32767;2:data_in=-1;3:data_in=1;4:data_in=$random;
   endcase
   for(j=5;j>0;j=j-1)history[j]=history[j-1];history[0]=data_in;
   total=history[2]+history[3]+history[4]+history[5];expected=total>>>2;
   @(posedge clk);#1;
   if(data_out!==expected)$fatal(1,"boxcar signed arithmetic/delay failed: %d %d",data_out,expected);
   if(i>=3)begin
    j=i-2;expected=j<10000 ? j*13 : 9999*13-(j-9999)*13;
    if(phase_out!==expected)$fatal(1,"phase unwrap/delay failed: %d %d at %d",phase_out,expected,i);
    if(wide_phase_out!==(longint'(expected)*256))$fatal(1,"wide phase unwrap scale/delay failed at %d",i);
   end
   if(valid0 && (random0!==random1 || random0!==random2 || random0!==random3))
    $fatal(1,"Expected current shared-seed sequence equality");
   if(valid0 && random0[0])ones=ones+1;
  end
  if(ones!=15176)$fatal(1,"Legacy LFSR sequence changed: ones=%d",ones);
  $display("FPGA block checks passed: signed boxcar, phase wrap in both directions, 2-cycle unwrap latency");
  $display("Confirmed: all four random rounding controls identical over 20000 cycles; ones=%d",ones);
  $finish;
 end
endmodule
