module LFSR (
    input clock,
    input reset,
    output [31:0] rnd 
    );
 
wire feedback = !(random[31] ^ random[21] ^ random[1] ^ random[0]); 
 
reg [31:0] random;

 
always @ (posedge clock or posedge reset)
begin
 if (reset)
 begin
  random <= 32'h007300F6; //An LFSR cannot have an all 0 state, thus reset to FF

 end
  
 

 else
 begin
  random <= {random[30:0], feedback}; //shift left the xor'd every posedge clock

 end
end
 

 
assign rnd = random;
 
endmodule
