// IEEE 1800-2017 23.8: a hierarchical reference inside a generate scope
// whose first component is an instance of an enclosing scope resolves
// upward (OpenTitan edn: `|u_edn_core.edn_enable_fo` in gen_edn_if_asserts)
module core(input clk, output [3:1] en_o);
	logic [3:1] en_q;            // non-zero LSB, three bits
	always @(posedge clk) en_q <= 3'b101;
	assign en_o = en_q;
	logic flag;
	assign flag = 1'b1;
endmodule
module leaf(input clk);
	logic s;
	assign s = 1'b1;
endmodule
module top(input clk);
	core u_core(.clk(clk), .en_o());
	for (genvar i = 0; i < 2; i++) begin : g_loop
		// upward, wide: reads the real three bits
		always @(posedge clk) a_wide: assert (u_core.en_q == 3'b000 || u_core.en_q == 3'b101);
		// upward, from a nested scope
		if (1) begin : g_in
			always @(posedge clk) a_nest: assert (u_core.flag);
		end
		// local instance: stays in this scope
		leaf u_leaf(.clk(clk));
		always @(posedge clk) a_local: assert (u_leaf.s);
	end
endmodule
