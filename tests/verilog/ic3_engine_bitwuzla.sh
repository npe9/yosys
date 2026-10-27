#!/usr/bin/env bash
# ic3 -engine bitwuzla (register E8): Bitwuzla through its C API, when this
# yosys was built with it; otherwise every property is undecided with the
# reason logged.
set -eu
trap 'rm -f ic3_engine_bitwuzla.tmp.v' EXIT
cat > ic3_engine_bitwuzla.tmp.v <<'EOT'
module t(input clk, input a, input [7:0] din);
  reg [2:0] c = 0;
  reg [8:0] d9 = 0;
  // false loops of the Functional IR: slices of one wire feed each other,
  // and the bits of one wide cell feed each other (a chain inside one
  // used slice)
  wire [3:0] w;
  assign w[0] = a;
  assign w[1] = w[0] ^ c[0];
  assign w[2] = w[1] & c[1];
  assign w[3] = w[2] | w[0];
  wire [3:0] n;
  assign n = ~{n[2:0], c[2]};
  always @(posedge clk) begin
    c <= c + a;
    d9 <= {1'b0, din};
    assume (a || c != 3'd5);
    p_c6: assert (c != 3'd6);
    p_c5: assert (c != 3'd5);
    p_x: assert (d9[11:0] == d9[11:0]);
    p_w: assert (w[3] == (w[2] | a));
    p_n: assert (n[3] == ~n[2]);
    c_4: cover (c == 3'd4);
  end
endmodule
EOT
prep="read_verilog -sv -formal ic3_engine_bitwuzla.tmp.v; prep -top t; async2sync"
if ${YOSYS} -p "$prep; ic3 -engine bitwuzla" 2>&1 | grep -q "built without Bitwuzla"; then
	${YOSYS} -q -p "$prep; logger -expect log \"engine bitwuzla: not run: .*built without Bitwuzla\" 1; ic3 -engine bitwuzla; logger -check-expected"
	exit 0
fi
# verdicts and depths as the native engine gives them; proofs certified
# on the AIGER model; no false loop refused
${YOSYS} -q -p "$prep
logger -expect log \"p_c6: disproved \\(depth 7\\)\" 1
logger -expect log \"p_c5: disproved \\(depth 6\\)\" 1
logger -expect log \"p_x: proved .*\\[certified\\]\" 1
logger -expect log \"p_w: proved .*\\[certified\\]\" 1
logger -expect log \"p_n: proved .*\\[certified\\]\" 1
logger -expect log \"c_4: cover reached \\(depth 5\\)\" 1
logger -expect log \"native proofs: 3 certified, 0 uncertified\" 1
ic3 -engine bitwuzla -certify
logger -check-expected"
# a deep counterexample past -depth is bounded, not a proof
${YOSYS} -q -p "$prep
logger -expect log \"p_c6: engine bitwuzla: no counterexample and no induction up to depth 3\" 1
logger -expect log \"p_c6: undecided\" 1
ic3 -engine bitwuzla -depth 3
logger -check-expected"
