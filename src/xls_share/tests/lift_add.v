module tc(input [7:0] a,b,c,d,e,f, input s,
output [7:0] add_y,sub_l,sub_r,reject_y,extra);
assign add_y=s ? (a+b):(a+c);
assign sub_l=s ? (a-b):(a-c);
assign sub_r=s ? (b-a):(c-a);
wire [7:0] p=d+e, q=d+f;
assign reject_y=s?p:q;
assign extra=p+q;
endmodule
