module LFSR (
    input clock,
    input reset,
    output [15:0] rnd 
    );
 
wire feedback = random[15] ^ random[14] ^ random[12] ^ random[3]; 
 
reg [15:0] random, random_done;
reg [3:0] count; //to keep track of the shifts
 
always @ (posedge clock or posedge reset)
begin
 if (reset)
 begin
  random <= 16'h73F6; //An LFSR cannot have an all 0 state, thus reset to FF

  count <= 0;
 end
  
 else if (count == 4)
// if count was 16 would need to increase count width to 5!
 begin
  count <= 0;

  random <= {random[14:0], feedback}; //shift left the xor'd every posedge clock

  random_done <= random; //assign the random number to output after only 5 shifts (might have needed to do 16, but hey should still be pretty random)

 end

 else
 begin
  random <= {random[14:0], feedback}; //shift left the xor'd every posedge clock

  count<= count + 1;

 end
end
 
//always @ (*)
//begin
// random_next = random; //default state stays the same
// count_next = count;
   
//  random_next = {random[14:0], feedback}; //shift left the xor'd every posedge clock
//  count_next = count + 1;
 

  
//end
 
 
assign rnd = random_done;
 
endmodule
