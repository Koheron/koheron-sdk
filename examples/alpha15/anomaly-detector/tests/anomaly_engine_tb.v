`timescale 1ns/1ps
module anomaly_engine_tb;
reg clk=0; always #2.083 clk=~clk;
reg resetn=0, valid=0, capture=0, arm=0, enable=0, commit=0;
reg [17:0] sample=0;
reg [31:0] threshold=200;
reg [31:0] w0=0,w1=0,w2=0,w3=0,bias=0;
wire [31:0] tagged,score,trigger,sequence;
wire tvalid,alert,triggered,done;
realtime pulse_adc_edge=0, pulse_alert_edge=0;
always @(posedge clk) if (valid && sample==8204) pulse_adc_edge=$realtime;
always @(posedge alert) if (pulse_adc_edge!=0 && pulse_alert_edge==0) pulse_alert_edge=$realtime;
anomaly_engine dut(.clk(clk),.resetn(resetn),.sample(sample),.sample_valid(valid),
 .capture_enable(capture),.arm(arm),.model_enable(enable),.threshold(threshold),
 .commit(commit),.weight0(w0),.weight1(w1),.weight2(w2),.weight3(w3),
 .bias(bias),
 .tagged_sample(tagged),.tagged_valid(tvalid),.alert(alert),.score(score),
 .trigger_sequence(trigger),.triggered(triggered),.capture_done(done),.sequence_status(sequence));

task conversion(input [17:0] value);
 integer j;
 begin
  @(negedge clk); sample=value; valid=1;
  @(negedge clk); valid=0;
  for (j=0;j<15;j=j+1) @(negedge clk);
 end
endtask
initial begin
 repeat(5) @(negedge clk); resetn=1; capture=1;
 // Output weights realize identity of the previous sample: ReLU(x)-ReLU(-x).
 w0=32'hf0001000; commit=1; enable=1;
 conversion(0); conversion(4); conversion(8);
 conversion(12);
 if (score !== 4 || alert !== 0) $fatal(1,"reference prediction failed score=%d",score);
 conversion(8204);
 if (score < 8000 || alert !== 1) $fatal(1,"pulse not detected score=%d",score);
 if (pulse_alert_edge-pulse_adc_edge < 8.2 || pulse_alert_edge-pulse_adc_edge > 8.5)
   $fatal(1,"unexpected alert latency: %t ns",pulse_alert_edge-pulse_adc_edge);
 conversion(0);
 if (score < 8000 || alert !== 1) $fatal(1,"pulse not detected score=%d",score);
 // Runtime update without reset: program zero weights and centered bias.
 w0=0;bias=0;commit=0;
 repeat(4) @(negedge clk);
 if (dut.settle==0) $fatal(1,"weight update did not start settling guard");
 conversion(8204);
 if (alert !== 0) $fatal(1,"weight transition emitted a false alert");
 conversion(0); conversion(0); conversion(0);
 if (score !== 0 || alert !== 0) $fatal(1,"runtime update failed score=%d",score);
 // Exercise trigger bookkeeping without simulating all 15 million samples.
 arm=1;
 force dut.history_count=15000000;
 conversion(8192);
 if (!triggered || !alert) $fatal(1,"trigger did not latch");
 release dut.history_count;
 force dut.post_count=15000000;
 force dut.sequence=32'h0001ffff;
 conversion(8192);
 if (!done || tvalid) $fatal(1,"capture did not stop at packet boundary");
 release dut.post_count;
 release dut.sequence;
 $display("PASS prediction, anomaly, guarded runtime weights and packet-aligned trigger");
 $finish;
end
endmodule
