module tc(input signed [3:0] a, input signed [5:0] b,c,d,
input [1:0] sel, input s,
output signed [7:0] y_signed, output [4:0] y_unsigned,
output reg signed [7:0] y_multi, output signed [7:0] y_alu);
assign y_signed=s?(a*b):(a*c);
wire [3:0] ua=a;
wire [5:0] ub=b, uc=c;
assign y_unsigned=s?(ua*ub):(ua*uc);
always @* case(sel)
0: y_multi=a*b;
1: y_multi=a*c;
default: y_multi=a*d;
endcase
assign y_alu=s?(a+b):(a+c);
endmodule
