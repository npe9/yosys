// IEEE 1800-2017 27.5: a conditional generate construct that is the only item
// of a generate block, without begin/end, is directly nested; its blocks belong
// to the outer construct and the wrapper is not a scope.
module top #(parameter int D = 4, parameter bit A = 1, parameter bit B = 1);
	if (D == 0) begin : g_zero
		logic w;
	end else if (D == 1) begin : g_one
		logic w;
	end else begin : g_many         // top.g_many.w, not top.genblk1.g_many.w
		logic w;
	end

	if (A)
		if (B) begin : g_then       // top.g_then.w
			logic w;
		end

	case (D)
		4: if (A) begin : g_arm     // top.g_arm.w
			logic w;
		end
		default: begin : g_dflt
			logic w;
		end
	endcase

	if (!A) begin : g_no
		logic w;
	end else begin                  // begin/end: a scope, implicitly genblk4
		if (B) begin : g_inner      // top.genblk4.g_inner.w
			logic w;
		end
	end

	// implicit names: the chain is the fifth construct, the directly nested
	// if adds none, so both arms are genblk5 and the next construct is genblk6
	if (!A) logic a;                // (top.genblk5.a)
	else if (B) logic b;            // top.genblk5.b
	if (1) logic c;                 // top.genblk6.c
endmodule
