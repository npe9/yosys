// IEEE 1800-2017 23.8: a checker bound into its target reads the target's
// signals by the target's module name (caliptra fv_coverpoints_m:
// `bind sha512_core fv_coverpoints_m ..` reading `sha512_core.zeroize`)
module sub(input clk, output logic [3:0] cnt);
	assign cnt = 4'd9;
endmodule
module dut #(parameter int W = 2) (input clk, input rst_n);
	logic zeroize;
	assign zeroize = 1'b0;
	logic [W-1:0] mode;
	assign mode = W'(2);
	sub u_sub(.clk(clk), .cnt());
endmodule
module chk_m(input clk, input rst_n);
	a_z: assert property (@(posedge clk) !dut.zeroize);
	a_m: assert property (@(posedge clk) dut.mode == 3'd2);      // W = 3 below
	a_d: assert property (@(posedge clk) dut.u_sub.cnt == 4'd9); // up, then down
endmodule
bind dut chk_m u_chk(.clk(clk), .rst_n(rst_n));
module top(input clk, input rst_n);
	dut #(.W(3)) u_dut(.clk(clk), .rst_n(rst_n));
endmodule
