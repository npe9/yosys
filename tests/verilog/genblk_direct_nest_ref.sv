// a hierarchical reference through an else-if chain (the OpenTitan
// prim_fifo_sync gen_normal_fifo shape) reaches its target
module cnt(input clk, output err_o);
	logic unused_assert_connected;
	assign err_o = 1'b0;
	always @(posedge clk) a_conn: assert (unused_assert_connected === 1'b1);
endmodule
module fifo #(parameter int Depth = 4)(input clk);
	if (Depth == 0) begin : gen_passthru
	end else if (Depth == 1) begin : gen_singleton_fifo
	end else begin : gen_normal_fifo
		cnt u_wptr(.clk(clk), .err_o());
	end
endmodule
module ref_top(input clk);
	fifo u_fifo(.clk(clk));
	assign u_fifo.gen_normal_fifo.u_wptr.unused_assert_connected = 1'b1;
	always @(posedge clk) a_err: assert (!u_fifo.gen_normal_fifo.u_wptr.err_o);
endmodule
