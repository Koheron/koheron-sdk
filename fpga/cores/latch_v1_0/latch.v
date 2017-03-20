module latch (
clk , 
set , 
reset , 
q 
);

input clk, set, reset ; 


output q;


reg q;

always @ ( posedge clk )
if (reset) begin
q <= 1'b0;
end else if (set) begin
q <= 1'b1;
end

endmodule
