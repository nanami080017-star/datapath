module macc_sum(input [3:0] a,b,c,d,e,f, input s,
                output [7:0] y,z);
  wire [7:0] p=a+b+c+a, q=d+e+f;
  assign y=s?p:q;
  assign z=s?(p^8'h35):(q^8'h79);
endmodule

module macc_product(input signed [3:0] a,b,c,d,e,f,g,h, input s,
                    output [7:0] y,z);
  wire signed [7:0] p=a*b+c*d-e, q=f*g+h+h;
  assign y=s?p:q;
  assign z=s?(p^8'h35):(q^8'h79);
endmodule

module macc_dependency(input [3:0] a,b,c,d,e, input s,
                       output [7:0] y,z);
  wire [7:0] p=a+b+c, q=p+d+e;
  assign y=s?p:q;
  assign z=s?(p^8'h35):(q^8'h79);
endmodule

module lift_commute(input [3:0] a,b,c, input s, output [7:0] y,tap);
  wire [7:0] p=a+b, q=c+a;
  assign y = s ? p : q;
  assign tap = p;
endmodule
module lift_identity_add(input [7:0] a,b,input s,output [7:0] y);
  assign y=s?(a+b):a;
endmodule
module lift_identity_sub(input [7:0] a,b,input s,output [7:0] y);
  assign y=s?(a-b):a;
endmodule
module lift_noncommute(input [7:0] a,b,c,input s,output [7:0] y);
  assign y=s?(a-b):(c-a);
endmodule

module width_mul(input signed [3:0] a,b,input signed [5:0] c,d,input s,
                 output [7:0] y,output [11:0] z);
  wire signed [7:0] p=a*b;
  wire signed [11:0] q=c*d;
  assign y=s?p:q[7:0];
  assign z=s?{{4{p[7]}},p}:q;
endmodule
module width_macc(input [3:0] a,b,c,input [5:0] d,e,f,input s,
                  output [7:0] y,output [11:0] z);
  wire [7:0] p=a+b+c;
  wire [11:0] q=d+e+f;
  assign y=s?p:q[7:0];
  assign z=s?{4'b0,p}:q;
endmodule
module mixed_add_sub(input [7:0] a,input [3:0] b,
                     input signed [5:0] c,input signed [7:0] d,input s,
                     output [39:0] y,output [7:0] z);
  wire [39:0] p=a-b;
  wire signed [35:0] q=c+d;
  assign y=s?p:{{4{q[35]}},q};
  assign z=s?(p[7:0]^8'h35):(q[7:0]^8'h79);
endmodule
module lift_nested_identity(input [7:0] a,b,c,input s,output [7:0] y);
  wire [7:0] p=a+b;
  assign y=s?(p+c):p;
endmodule

module parallel_inputs(input [3:0] a,b,c,d,e,f, input s,t,
                       output [7:0] y,z);
  wire [7:0] p=a*b, q=c*d, r=e*f;
  assign y=s?p:(t?q:r);
  assign z=s?(p^8'h13):(t?(q^8'h57):(r^8'h9b));
endmodule

module conditional_edge(input [7:0] a,b,c,input r,s,output [7:0] y,z);
  wire [7:0] u=(r|s)?a:b;
  assign y=r?u:c;
  assign z=u;
endmodule
module conditional_no_imply(input [7:0] a,b,c,input r,s,t,output [7:0] y,z);
  wire [7:0] u=(r|s)?a:b;
  assign y=t?u:c;
  assign z=u;
endmodule

module cost_mux_constant(input [1:0] a,b,c,d,e,f, input s,
                         output [1:0] y,z);
  wire [1:0] p=a+b+f+2'd2, q=c+d+e;
  assign y=s?p:q;
  assign z=s?(p^2'd1):(q^2'd2);
endmodule

module conditional_pmux(input [1:0] code, input [7:0] a,b,c,d,
                        output [7:0] y,z);
  reg [7:0] u;
  always @* case (code)
    2'b00: u = a;
    2'b01: u = b;
    default: u = c;
  endcase
  assign y = (code == 2'b01) ? u : d;
  assign z = u;
endmodule

module conditional_known(input r, input [7:0] a, output [7:0] y);
  assign y = r ? {a[7:1], r} : a;
endmodule

module mask_control_share(input [5:0] a,b,c,d, input s,t,u,
                          output [11:0] y,z);
  wire [11:0] p=a*b, q=c*d;
  assign y = (p & {12{s}}) & {12{t}};
  assign z = (q | {12{s}}) | {12{~u}};
endmodule

module select_chain_pack(input [1:0] code, input [7:0] a,b, output [7:0] y);
  wire [7:0] u = (code == 2'b00) ? a : 8'b0;
  assign y = (code == 2'b01) ? b : u;
endmodule

module select_chain_polarity(input [1:0] code, input a,b,d, output y);
  wire u = (code == 2'b00) ? a : d;
  assign y = (code == 2'b01) ? u : b;
endmodule

module select_chain_live_middle(input [1:0] code, input [7:0] a,b,
                                output [7:0] y,z);
  wire [7:0] u = (code == 2'b00) ? a : 8'b0;
  assign y = (code == 2'b01) ? b : u;
  assign z = u;
endmodule

module select_chain_hidden_x(input p, input [15:0] b,d, output [15:0] y);
  wire c = p | 1'bx;
  wire [15:0] inner = c ? b : 16'b0;
  assign y = p ? inner : d;
endmodule

module cost_repeated_input(input s, input [1:0] a,b,c,d, output [1:0] y);
  wire [1:0] p = a + b + a, q = c + d + c;
  assign y = s ? q : p;
endmodule

module cost_truncated_product(input s, input [7:0] a,b,c,d, output [1:0] y);
  wire [1:0] p = a * b, q = c * d;
  assign y = s ? q : p;
endmodule

module rewrite_competition(input [7:0] a,b,c,d, input [5:0] e,f,g,h,
                           input [1:0] code, input s,t,r,
                           output [7:0] lifted, live_pick, tap, packed,
                           output [11:0] shared0, shared1);
  assign lifted = r ? a+b : a;
  wire [7:0] p=c+b, q=d+b;
  assign live_pick = s ? p : q;
  assign tap = p;
  wire [11:0] u=e*f, v=g*h;
  assign shared0 = t ? u : v;
  assign shared1 = t ? u^12'h135 : v^12'h579;
  wire [7:0] inner = (code == 2'b00) ? a : 8'b0;
  assign packed = (code == 2'b01) ? d : inner;
endmodule

module narrow_share(input [7:0] a,b,c,d, input s, output [7:0] y,z);
  wire [7:0] p=a+b, q=c-d;
  assign y = s ? p : q;
  assign z = s ? p^8'h35 : q^8'h79;
endmodule

module lift_control_operand(input [7:0] a,b, output [7:0] y);
  wire s = a[0] ^ a[1];
  assign y = s ? a+b : a;
endmodule

module lift_control_unknown(input [7:0] a,b, output [7:0] y);
  wire s = a[0] | 1'bx;
  assign y = s ? a+b : a;
endmodule

module six_groups(input [14:0] d, input [11:0] a, input [1:0] s, output [26:0] y);
  wire ua = s == 2'd0, ub = s == 2'd1, uc = s == 2'd2;
  wire [4:0] da = d[2:0]*d[4:3], db = d[7:5]*d[9:8], dc = d[12:10]*d[14:13];
  wire [3:0] pa = a[1:0]*a[3:2], pb = a[5:4]*a[7:6], pc = a[9:8]*a[11:10];
  assign y = {
    (ub | uc) ? da : 5'b0, (ua | uc) ? db : 5'b0, (ua | ub) ? dc : 5'b0,
    ua ? pa : 4'b0, ub ? pb : 4'b0, uc ? pc : 4'b0
  };
endmodule

module lift_identity_unknown(input [6:0] u, input [3:0] b, input p,s, output [7:0] y);
  wire [7:0] a = {p | 1'bx,u};
  assign y = s ? a+b : a;
endmodule
