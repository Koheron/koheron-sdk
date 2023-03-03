`timescale 1 ns / 1 ps

module integrator #
(
  parameter integer ADC_WIDTH = 18,
  parameter integer DAC_WIDTH = 16
)
(
  input  wire adc_valid,
  input  wire clk,
  input  wire integrate,
  input wire [ADC_WIDTH - 1:0] setpoint,
  input wire [ADC_WIDTH - 1:0] adc_data,
  output wire [DAC_WIDTH - 1:0] dac_data
);

  reg [ADC_WIDTH - 1:0] sum;
  initial sum = 0;
  
  reg adc_valid_reg;
  reg integrate_reg;

  always @(posedge clk) begin
    adc_valid_reg <= adc_valid;
    integrate_reg <= integrate;
    
    if (integrate && (adc_valid && !adc_valid_reg)) begin
      //error <= setpoint - adc_data_reg;
      sum <= sum + (setpoint - adc_data);
    end
  end
 
  always @(posedge clk) begin
    if (!integrate) begin
      sum <= setpoint;
    end
  end
   
  always @(posedge clk) begin
     // Reset the sum on integrate pos edge
     if (integrate && !integrate_reg) begin
       sum <= 0;
     end
  end

  assign dac_data = sum;

endmodule
