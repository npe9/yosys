module dut(input clk);
	logic zeroize;
	assign zeroize = 1'b0;
endmodule
module other(input clk);
	logic zeroize;
	assign zeroize = 1'b0;
endmodule
// writes its upward reference: not turned into an input port
module wr_m(input clk);
	assign dut.zeroize = 1'b1;
endmodule
// placed under two different module types: `dut.` names only one of them
module two_m(input clk);
	a_two: assert property (@(posedge clk) !dut.zeroize);
endmodule
bind dut wr_m u_wr(.clk(clk));
bind dut two_m u_two(.clk(clk));
bind other two_m u_two(.clk(clk));
module top(input clk);
	dut u_dut(.clk(clk));
	other u_other(.clk(clk));
endmodule
