module OneClockPulse (
clk , 
trig , 
pulse
 
);

input clk, trig ; 


output pulse;

reg previous,pulse;


always @ ( posedge clk ) begin
previous <= trig;
pulse <= trig & !previous;
end



endmodule
