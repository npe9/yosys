// a member select on an imported struct-typed package parameter
// (OpenTitan dma/mbx: `.wdata_intg_i(TL_A_USER_DEFAULT.data_intg)` after
// `import tlul_pkg::*;`) reads the parameter's value, not a free wire
package sp_pkg;
	typedef struct packed { logic [1:0] lo; logic [2:0] hi; } in_t;
	typedef struct packed { logic [3:0] rsvd; in_t sub; logic [6:0] data_intg; } u_t;
	parameter u_t DEF = '{rsvd: '0, sub: '{lo: 2'b10, hi: 3'b011}, data_intg: {7{1'b1}}};
	parameter u_t OTHER = '{rsvd: 4'h9, sub: '0, data_intg: 7'h2a};
endpackage
module sink(input [6:0] d, output [6:0] q);
	assign q = d;
endmodule
module wild(output [6:0] a, output [2:0] h, output [6:0] q);
	import sp_pkg::*;
	assign a = DEF.data_intg;
	assign h = DEF.sub.hi;
	sink u_sink(.d(DEF.data_intg), .q(q));
	always_comb begin
		a_wild: assert (a == 7'h7f);
		a_nest: assert (h == 3'b011);
		a_port: assert (q == 7'h7f);
	end
endmodule
module named(output [6:0] b, output [6:0] c);
	import sp_pkg::OTHER;
	assign b = OTHER.data_intg;
	assign c = sp_pkg::DEF.data_intg;
	always_comb begin
		a_named: assert (b == 7'h2a);
		a_qual: assert (c == 7'h7f);
	end
endmodule
