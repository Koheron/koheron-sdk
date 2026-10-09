`timescale 1ns / 1ps
module dac_output_buffer_tb;
    parameter real PERIOD_NS = 4.0;
    parameter real PHASE_NS = 0.0;
    reg adc_clk = 0, dac_clk = 0, reset = 1;
    initial begin
        #(PHASE_NS);
        forever #(PERIOD_NS / 2.0) adc_clk = ~adc_clk;
    end
    always #(PERIOD_NS / 2.0) dac_clk = ~dac_clk;
    initial begin
        repeat (8) @(negedge adc_clk);
        reset = 0;
        repeat (250) @(negedge adc_clk);
        reset = 1;
        repeat (8) @(negedge adc_clk);
        reset = 0;
    end
    reg [15:0] input_word = 16'h5a81, delay1 = 0, delay2 = 0;
    reg [15:0] original_output = 0, buffered_output = 0;
    wire [15:0] buffered_word, second_word;
    reg buffer_valid = 0, io_valid = 0;
    reg [15:0] expected[$];
    reg [15:0] history [0:63];
    reg [15:0] expected_word;
    integer captures = 0, checked = 0, lag = -1, match_lag, epoch;
    integer latency [0:1];
    initial begin
        latency[0] = -1; latency[1] = -1;
        for (integer i = 0; i < 64; i = i + 1) history[i] = 0;
    end
    dac_output_buffer dut (
        .adc_clk(adc_clk), .dac_clk(dac_clk), .reset(reset),
        .din(input_word), .dout0(buffered_word), .dout1(second_word)
    );
    always @(posedge reset) expected.delete();
    always @(posedge adc_clk) begin
        input_word <= {input_word[14:0],
                       input_word[15] ^ input_word[13] ^ input_word[12] ^ input_word[10]};
        delay1 <= input_word; delay2 <= delay1;
        if (!reset && !dut.wr_busy) begin
            if (dut.fifo_full) $fatal(1, "FIFO dropped an ADC-clock sample");
            expected.push_back(input_word);
        end
    end
    always @(posedge dac_clk) begin
        original_output <= delay2;
        buffered_output <= buffered_word;
        buffer_valid <= !reset && !dut.rd_busy && !dut.fifo_empty;
        io_valid <= !reset && buffer_valid;
    end
    always @(negedge dac_clk) begin
        if (buffered_word !== second_word) $fatal(1, "DAC channels lost sample alignment");
        captures = captures + 1;
        history[captures % 64] = original_output;
        if (!reset && io_valid) begin
            if (expected.size() == 0) $fatal(1, "Unexpected DAC word");
            expected_word = expected.pop_front();
            if (buffered_output !== expected_word)
                $fatal(1, "DAC word mismatch: %h != %h", buffered_output, expected_word);
            checked = checked + 1;
        end
        if ((captures > 100 && captures < 250) || captures > 380) begin
            if (!io_valid) $fatal(1, "Gap in steady DAC sample stream");
            match_lag = -1;
            for (integer i = 0; i < 16; i = i + 1)
                if (buffered_output === history[(captures - i) % 64]) match_lag = i;
            if (match_lag < 0) $fatal(1, "DAC sample latency unavailable");
            epoch = captures > 380 ? 1 : 0;
            if (latency[epoch] < 0) latency[epoch] = match_lag;
            if (match_lag != latency[epoch]) $fatal(1, "DAC latency changed in steady stream");
        end
        if (captures == 650) begin
            if (checked < 500) $fatal(1, "Too few verified DAC samples");
            $display("PASS: %0d ordered DAC samples across two resets, period=%0f ns phase=%0f ns extra_latency=%0d/%0d cycles",
                     checked, PERIOD_NS, PHASE_NS, latency[0], latency[1]);
            $finish;
        end
    end
endmodule
