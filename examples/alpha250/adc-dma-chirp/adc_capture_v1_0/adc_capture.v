// ADC cannot be backpressured: any lost word invalidates the acquisition.
module adc_capture #(
    parameter integer N_SAMPLES = 67108864,
    parameter integer PACKET_BEATS = 32768,
    parameter integer TAPER_BITS = 18
) (
    input wire clk,
    input wire aresetn,
    input wire [15:0] sine,
    input wire sine_valid,
    input wire [31:0] sample_count,
    output reg [15:0] dac,
    input wire [15:0] adc,
    output wire [31:0] status,
    output reg [63:0] m_axis_tdata,
    output reg m_axis_tvalid,
    input wire m_axis_tready,
    output reg m_axis_tlast,
    output wire [7:0] m_axis_tkeep
);
    reg active, done, overflow, clipped;
    reg [31:0] count;
    reg [31:0] beat;
    reg [47:0] pack;
    // Short linear tapers suppress the start/stop discontinuities. Align
    // acquisition with the first DAC sample, independent of DDS LUT latency.
    reg [31:0] sine_count, end_count, count0, remaining0;
    reg [15:0] sine0, sine1;
    reg [TAPER_BITS:0] gain1;
    reg signed [TAPER_BITS+17:0] product, product_d;
    reg [3:0] sine_valid_d;
    reg dac_valid, dac_valid_d;
    wire start = dac_valid && !dac_valid_d;
    always @(posedge clk) begin
        sine_count <= sine_valid ? sine_count + 1'b1 : 0;
        // Register subtraction, taper selection and multiplication separately.
        end_count <= sample_count - 1;
        count0 <= sine_count;
        remaining0 <= end_count - sine_count;
        sine0 <= sine;
        sine1 <= sine0;
        gain1 <= (count0 < (1 << TAPER_BITS)) ? count0[TAPER_BITS:0] :
                 (remaining0 < (1 << TAPER_BITS)) ? remaining0[TAPER_BITS:0] : (1 << TAPER_BITS);
        product <= $signed(sine1) * $signed({1'b0, gain1});
        product_d <= product;
        sine_valid_d <= {sine_valid_d[2:0], sine_valid};
        dac <= sine_valid_d[3] ? (product_d >>> TAPER_BITS) : 0;
        dac_valid <= sine_valid_d[3];
        dac_valid_d <= dac_valid;
        if (!aresetn) begin
            sine_count <= 0; sine_valid_d <= 0;
            dac <= 0; dac_valid <= 0; dac_valid_d <= 0;
        end
    end
    assign status = {28'b0, clipped, overflow, done, active};
    assign m_axis_tkeep = 8'hff;
    always @(posedge clk) begin
        if (!aresetn) begin
            active <= 0; done <= 0; overflow <= 0; clipped <= 0;
            count <= 0; beat <= 0; pack <= 0;
            m_axis_tvalid <= 0; m_axis_tlast <= 0; m_axis_tdata <= 0;
        end else begin
            if (m_axis_tready) m_axis_tvalid <= 0;
            // start marks sample zero at the digital DAC port.
            if (start && !active && !done) begin
                active <= 1;
                pack[15:0] <= adc;
                count <= 1;
                clipped <= ($signed(adc) >= 32752 || $signed(adc) <= -32752);
            end else if (active) begin
                count <= count + 1'b1;
                if ($signed(adc) >= 32752 || $signed(adc) <= -32752) clipped <= 1;
                case (count[1:0])
                    0: pack[15:0] <= adc;
                    1: pack[31:16] <= adc;
                    2: pack[47:32] <= adc;
                    3: begin
                        if (m_axis_tvalid && !m_axis_tready) begin
                            overflow <= 1;
                            active <= 0;
                        end else begin
                            m_axis_tdata <= {adc, pack};
                            m_axis_tvalid <= 1;
                            m_axis_tlast <= (beat == PACKET_BEATS-1);
                            beat <= (beat == PACKET_BEATS-1) ? 0 : beat+1;
                        end
                    end
                endcase
                if (count == N_SAMPLES-1) begin
                    active <= 0;
                    done <= 1;
                end
            end
        end
    end
endmodule
