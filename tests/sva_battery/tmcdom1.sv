// SVA2CHECK-FLAGS: -clk2fflogic
// Two-clock req/ack handshake: v1 multiclock SVA test.
//
// DUT: req_pulse (clk_a domain) sets sticky req.  The ack domain captures
// req through a 2-stage synchronizer and ack (registered level) rises one
// clk_b tick after the synchronized level -- so a request event is acked
// from the 3rd clk_b tick of the b-domain's first sight of req: inside
// ##[1:4] but never at ##[1:1] (##1 only reaches the 2nd clk_b tick).
// req never clears, so there is exactly one request episode.
// p_hand_bad fails at step 5 (was @4 before -clk2fflogic: per-step
// evaluation): clk_a ticks at 1 (req set) and 3 ($rose), a coinciding
// clk_b tick at 3 starts the |-> consequent, ##1 reaches the clk_b tick
// at 5 where the sampled ack is still 0.
//
// PREP-EXTRA-AFTER-MEMORY: clk2fflogic
// IC3-TIMEOUT: 3600
module tmcdom1(input logic clk_a, input logic clk_b, input logic req_pulse);

	logic req;
	logic syn0, syn1;
	logic ack_r, ack;

	// request domain (clk_a): sticky one-shot request
	always @(posedge clk_a)
		if (req_pulse)
			req <= 1'b1;

	// ack domain (clk_b): 2-stage synchronizer + registered ack level
	always @(posedge clk_b) begin
		syn0  <= req;
		syn1  <= syn0;
		ack_r <= syn1;
	end
	assign ack = ack_r;

	p_hand_ok:  assert property (@(posedge clk_a) $rose(req) |-> strong(@(posedge clk_b) ##[1:4] ack)); // PROVED
	p_hand_bad: assert property (@(posedge clk_a) $rose(req) |-> strong(@(posedge clk_b) ##[1:1] ack)); // DIS @5

endmodule
