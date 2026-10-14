`timescale 1ns/1ps
// Register map: DATA=0, STATUS=4, IRQ_ENABLE=8, RESET=12,
// TX_BYTES=16, RX_BYTES=20, ERRORS=24, FRAME_CYCLES=28,
// CLOCK=32, IRQ_CLOCK=36. IRQ is level sensitive; service before rearming UIO.
module bist_uart #(
    parameter integer CLOCK_HZ = 125000000,
    parameter integer BAUD = 460800,
    parameter integer FIFO_BITS = 6
)(
    input wire aclk, input wire aresetn,
    input wire [11:0] s_axi_awaddr, input wire s_axi_awvalid, output wire s_axi_awready,
    input wire [31:0] s_axi_wdata, input wire [3:0] s_axi_wstrb,
    input wire s_axi_wvalid, output wire s_axi_wready,
    output wire [1:0] s_axi_bresp, output reg s_axi_bvalid, input wire s_axi_bready,
    input wire [11:0] s_axi_araddr, input wire s_axi_arvalid, output wire s_axi_arready,
    output reg [31:0] s_axi_rdata, output wire [1:0] s_axi_rresp,
    output reg s_axi_rvalid, input wire s_axi_rready,
    input wire rx, output wire tx, output wire irq
);
    localparam integer DEPTH = 1 << FIFO_BITS;
    reg aw_hold, w_hold;
    reg [11:0] awaddr;
    reg [31:0] wdata;
    reg [3:0] wstrb;
    assign s_axi_awready = !aw_hold && !s_axi_bvalid;
    assign s_axi_wready = !w_hold && !s_axi_bvalid;
    assign s_axi_arready = !s_axi_rvalid;
    assign s_axi_bresp = 0;
    assign s_axi_rresp = 0;
    wire wr = aw_hold && w_hold && !s_axi_bvalid;
    wire rd = s_axi_arvalid && s_axi_arready;
    wire reset_uart = wr && awaddr[11:2] == 3 && wstrb[0] && wdata[0];
    reg [2:0] irq_enable;
    reg [7:0] tx_fifo[0:DEPTH-1], rx_fifo[0:DEPTH-1];
    reg [FIFO_BITS-1:0] tw, tr, rw, rr;
    reg [FIFO_BITS:0] tx_count, rx_count;
    reg [31:0] clock_count, irq_clock, tx_bytes, rx_bytes, errors, frame_cycles;
    reg irq_previous;
    assign irq = (irq_enable[0] && rx_count != 0) ||
                 (irq_enable[1] && tx_count <= DEPTH/2) || (irq_enable[2] && errors != 0);
    // An accumulator derives a 16x baud enable without assuming an integer divider.
    reg [31:0] phase;
    wire tick = phase >= CLOCK_HZ - BAUD*16;
    reg [9:0] tx_shift;
    reg [3:0] tx_bits, tx_sub;
    reg [31:0] frame_start;
    assign tx = tx_bits != 0 ? tx_shift[0] : 1'b1;
    wire tx_pop = tick && tx_bits == 0 && tx_count != 0;
    wire tx_push = wr && awaddr[11:2] == 0 && wstrb[0] && (tx_count < DEPTH || tx_pop);
    (* ASYNC_REG = "TRUE" *) reg [1:0] rx_sync;
    reg rx_active;
    reg [3:0] rx_sub, rx_bit;
    reg [7:0] rx_shift;
    wire rx_finish = tick && rx_active && rx_sub == 0 && rx_bit == 9;
    wire rx_pop = rd && s_axi_araddr[11:2] == 0 && rx_count != 0;
    wire rx_push = rx_finish && rx_sync[1] && (rx_count < DEPTH || rx_pop);
    always @(posedge aclk) begin
        if (!aresetn) begin
            aw_hold <= 0; w_hold <= 0; s_axi_bvalid <= 0; s_axi_rvalid <= 0;
            awaddr <= 0; wdata <= 0; wstrb <= 0; s_axi_rdata <= 0;
        end else begin
            if (s_axi_awvalid && s_axi_awready) begin aw_hold <= 1; awaddr <= s_axi_awaddr; end
            if (s_axi_wvalid && s_axi_wready) begin w_hold <= 1; wdata <= s_axi_wdata; wstrb <= s_axi_wstrb; end
            if (wr) begin aw_hold <= 0; w_hold <= 0; s_axi_bvalid <= 1; end
            if (s_axi_bvalid && s_axi_bready) s_axi_bvalid <= 0;
            if (s_axi_rvalid && s_axi_rready) s_axi_rvalid <= 0;
            if (rd) begin
                s_axi_rvalid <= 1;
                case (s_axi_araddr[11:2])
                    0: s_axi_rdata <= rx_count != 0 ? {24'b0,rx_fifo[rr]} : 0;
                    1: s_axi_rdata <= {14'b0,(tx_bits != 0),1'b0,{(7-FIFO_BITS){1'b0}},tx_count,{(7-FIFO_BITS){1'b0}},rx_count};
                    2: s_axi_rdata <= {29'b0,irq_enable};
                    4: s_axi_rdata <= tx_bytes;
                    5: s_axi_rdata <= rx_bytes;
                    6: s_axi_rdata <= errors;
                    7: s_axi_rdata <= frame_cycles;
                    8: s_axi_rdata <= clock_count;
                    9: s_axi_rdata <= irq_clock;
                    default: s_axi_rdata <= 0;
                endcase
            end
        end
        if (!aresetn || reset_uart) begin
            tw <= 0; tr <= 0; rw <= 0; rr <= 0; tx_count <= 0; rx_count <= 0;
            phase <= 0; tx_shift <= 10'h3ff; tx_bits <= 0; tx_sub <= 0;
            rx_sync <= 3; rx_active <= 0; rx_sub <= 0; rx_bit <= 0; rx_shift <= 0;
            irq_enable <= 0; clock_count <= 0; irq_clock <= 0; irq_previous <= 0;
            tx_bytes <= 0; rx_bytes <= 0; errors <= 0; frame_cycles <= 0; frame_start <= 0;
        end else begin
            clock_count <= clock_count + 1;
            irq_previous <= irq;
            if (irq && !irq_previous) irq_clock <= clock_count;
            rx_sync <= {rx_sync[0],rx};
            phase <= tick ? phase + BAUD*16 - CLOCK_HZ : phase + BAUD*16;
            if (wr && awaddr[11:2] == 2 && wstrb[0]) irq_enable <= wdata[2:0];
            if (tx_push) begin tx_fifo[tw] <= wdata[7:0]; tw <= tw + 1; end
            if (tx_pop) begin
                tr <= tr + 1; tx_shift <= {1'b1,tx_fifo[tr],1'b0};
                tx_bits <= 10; tx_sub <= 15; frame_start <= clock_count;
            end else if (tick && tx_bits != 0) begin
                if (tx_sub == 0) begin
                    tx_sub <= 15; tx_shift <= {1'b1,tx_shift[9:1]}; tx_bits <= tx_bits - 1;
                    if (tx_bits == 1) begin tx_bytes <= tx_bytes + 1; frame_cycles <= clock_count - frame_start; end
                end else tx_sub <= tx_sub - 1;
            end
            case ({tx_push,tx_pop})
                2'b10: tx_count <= tx_count + 1;
                2'b01: tx_count <= tx_count - 1;
                default: tx_count <= tx_count;
            endcase
            if (tick) begin
                if (!rx_active) begin
                    if (!rx_sync[1]) begin rx_active <= 1; rx_sub <= 7; rx_bit <= 0; end
                end else if (rx_sub != 0) rx_sub <= rx_sub - 1;
                else begin
                    rx_sub <= 15; rx_bit <= rx_bit + 1;
                    if (rx_bit == 0 && rx_sync[1]) rx_active <= 0;
                    if (rx_bit >= 1 && rx_bit <= 8) rx_shift[rx_bit-1] <= rx_sync[1];
                    if (rx_bit == 9) rx_active <= 0;
                end
            end
            if (rx_finish) begin
                rx_bytes <= rx_bytes + 1;
                if (!rx_sync[1] || (rx_count == DEPTH && !rx_pop)) errors <= errors + 1;
            end
            if (wr && awaddr[11:2] == 0 && tx_count == DEPTH && !tx_pop) errors <= errors + 1;
            if (rx_push) begin rx_fifo[rw] <= rx_shift; rw <= rw + 1; end
            if (rx_pop) rr <= rr + 1;
            case ({rx_push,rx_pop})
                2'b10: rx_count <= rx_count + 1;
                2'b01: rx_count <= rx_count - 1;
                default: rx_count <= rx_count;
            endcase
        end
    end
endmodule
