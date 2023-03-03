`timescale 1 ns / 1 ps

module ltc2387 #
(
)
(
  input wire clk,
  input wire din0_a,
  input wire din0_b,
  input wire din0_co,
  input wire din1_a,
  input wire din1_b,
  input wire din1_co,
  input wire clkout_dec,
  output reg clkout,
  output reg [17:0] adc0,
  output reg [17:0] adc1,
  output reg adc_valid
);

reg adc_ready;
initial adc_ready = 1;

reg [3:0] count;
initial count = 0;

reg [3:0] count_data_bits;
initial count_data_bits = 0;

reg din0_co_reg;
reg din0_a_reg;
reg din0_b_reg;
reg din0_a_reg_reg;
reg din0_b_reg_reg;

// reg din1_co_reg;
reg din1_a_reg;
reg din1_b_reg;
reg din1_a_reg_reg;
reg din1_b_reg_reg;

reg [4:0] addr_a_reg;
initial addr_a_reg = 17;
reg [4:0] addr_b_reg;
initial addr_b_reg = 16;

reg clkout_dec_reg;

always @(posedge clk) begin
  din0_co_reg <= din0_co;
  din0_a_reg <= din0_a;
  din0_b_reg <= din0_b;
  din0_a_reg_reg <= din0_a_reg;
  din0_b_reg_reg <= din0_b_reg;
  
  // din1_co_reg <= din1_co;
  din1_a_reg <= din1_a;
  din1_b_reg <= din1_b;
  din1_a_reg_reg <= din1_a_reg;
  din1_b_reg_reg <= din1_b_reg;
  
  // Reset count_data_bits at 1st DCO pulse train positive edge
  if (adc_ready && (din0_co && !din0_co_reg)) begin
    count_data_bits <= 0;
    addr_a_reg <= 17; 
    addr_b_reg <= 16; 
    adc_ready <= 0;
  end else begin
    count_data_bits <= count_data_bits + 1;
    addr_a_reg <= addr_a_reg - 2;
    addr_b_reg <= addr_b_reg - 2;

    if (count_data_bits == 9) begin
      adc_ready <= 1;
    end
  end
end

always @(posedge clk) begin
  if (!(clkout_dec && !clkout_dec_reg)) begin
    count <= count + 1;
  end
end
 
always @(posedge clk) begin
  if (count_data_bits < 9) begin
    adc0[addr_a_reg] <= din0_a_reg_reg;
    adc0[addr_b_reg] <= din0_b_reg_reg;
    
    adc1[addr_a_reg] <= din1_a_reg_reg;
    adc1[addr_b_reg] <= din1_b_reg_reg;
  end
end

always @(posedge clk) begin
  if (count < 10) begin
    clkout <= count[0];
  end else begin
    clkout <= 0;
  end
end

always @(posedge clk) begin
  if (count_data_bits == 9) begin
    adc_valid <= 1;
  end else begin
    adc_valid <= 0;
  end
end

endmodule