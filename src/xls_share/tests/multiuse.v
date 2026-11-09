module tc(input [7:0] a,b,c,d, input s,t,
 output [7:0] y0,y1,y2,y3,exposed);
 wire [7:0] p=a*b, q=c*d;
 wire [7:0] r=a*c, u=b*d;
 assign y0=s?p:q;
 assign y1=s?(p+8'd1):(q+8'd2);
 assign y2=s?r:u;
 assign y3=t?(r+8'd3):(u+8'd4);
 assign exposed=a*d;
endmodule
