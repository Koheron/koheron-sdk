`timescale 1ns/1ps
module dds_lut_tb;
    reg passed=0;
    reg clk=0;
    always #2 clk=~clk;
    reg aresetn=0, in_valid=0;
    reg [15:0] in_phase=0;
    wire out_valid, out_tag;
    wire [15:0] out_sine;
    test_sine_lut dut(.aclk(clk), .aresetn(aresetn),
        .s_axis_phase_tvalid(1'b1), .s_axis_phase_tdata(in_phase),
        .s_axis_phase_tuser(in_valid), .m_axis_data_tuser(out_tag),
        .m_axis_data_tvalid(out_valid), .m_axis_data_tdata(out_sine));
    integer i,received=0,expected,difference,max_difference=0;
    integer samples_file;
    real sine_value;
    always @(posedge clk) begin
        if(out_valid && out_tag) begin
            $fdisplay(samples_file,"%0d",$signed(out_sine));
            sine_value=32766.0*$sin(6.283185307179586*received/65536.0);
            expected=(sine_value>=0) ? $rtoi(sine_value+0.5) : $rtoi(sine_value-0.5);
            difference=$signed(out_sine)-expected;
            if(difference<0) difference=-difference;
            if(difference>max_difference) max_difference=difference;
            if(difference!=0) $fatal(1,"DDS/reference mismatch at phase %0d",received);
            received=received+1;
        end
    end
    initial begin
        samples_file=$fopen("dds_sine_samples.txt","w");
        repeat(8) @(negedge clk);
        aresetn=1;
        repeat(8) @(negedge clk);
        for(i=0;i<65536;i=i+1) begin
            in_valid=1;in_phase=i;
            @(negedge clk);
        end
        in_valid=0;
        repeat(32) @(negedge clk);
        if(received!=65536) $fatal(1,"DDS output count %0d",received);
        $display("PASS DDS LUT: all 65536 phases, reference error <= %0d LSB",max_difference);
        passed=1;
        $fclose(samples_file);
        $finish;
    end
    initial begin #300000; $fatal(1,"timeout"); end
endmodule
