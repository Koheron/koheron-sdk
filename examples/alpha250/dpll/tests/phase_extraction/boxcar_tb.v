`timescale 1 ns / 1 ps
module phase_extraction_boxcar_test;
    wire done;
    boxcar_filter_checker #(.DATA_WIDTH(24)) check24(done);
    initial begin
        wait(done);
        $display("24-bit boxcar checks passed: signed extrema, fractional IQ, averaging and latency");
        $finish;
    end
endmodule
