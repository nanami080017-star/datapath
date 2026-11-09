module tc(input [7:0] a,b,c,d,input s,
 output [7:0] y,z,output signed [7:0] signed_y);
 wire [7:0] p=a*b,q=c*d,r=a*c;
 assign y=s?p:q;
 assign z=r[0]?r:(b*d);
 assign signed_y=s?($signed(a)*$signed(d)):($signed(c)*$signed(b));
endmodule
