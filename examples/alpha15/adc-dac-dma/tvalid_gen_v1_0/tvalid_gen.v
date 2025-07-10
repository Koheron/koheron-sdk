`timescale 1 ns / 1 ps

module tvalid_gen(      
    input   wire valid,     // adc_valid
    input   wire tready,    // dma ready
    input   wire en,        // ps ctl signal
    output  wire tvalid     // axis valid 
);

    assign tvalid = tready && valid && en;

endmodule