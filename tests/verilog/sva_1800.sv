
module m(input clk, rst, a, b, c, d, req, gnt);
  sequence s_req;
    req ##1 gnt;
  endsequence
  property p_req(x, y);
    @(posedge clk) disable iff (rst)
      x |-> y;
  endproperty
  let ready = a && b;
  assert property (p_req(req, gnt));
  assert property (@(posedge clk) s_req);
  assert property (@(posedge clk) a ##[1:4] b |=> c until d);
  assert property (@(posedge clk) first_match(a ##1 b) |-> c);
  assert property (@(posedge clk) a intersect b ##1 c);
  assert property (@(posedge clk) a within (b ##1 c));
  assert property (@(posedge clk) not (a ##1 b[=2] ##1 c[->1]));
  assert property (@(posedge clk) a #-# b #=# c);
  assert property (@(posedge clk) a implies b iff c);
  assert property (@(posedge clk) nexttime [2] a);
  assert property (@(posedge clk) s_nexttime (always b));
  assert property (@(posedge clk) s_always [1:3] a);
  assert property (@(posedge clk) eventually [1:2] a);
  assert property (@(posedge clk) accept_on (rst) reject_on (b) a |=> c);
  assert property (@(posedge clk) sync_accept_on (rst) sync_reject_on (b) a);
  assert property (@(posedge clk) strong(a ##1 b));
  assert property (@(posedge clk) weak(a ##[+] b));
  assert property (@(posedge clk) if (rst) a else b |-> c);
  assert property (@(posedge clk) case (a)
    1'b1: b |=> c;
    default: c;
  endcase);
  cover sequence (@(posedge clk) a ##[*] b);
  assume property (@(posedge clk) a[+] |-> b);
  restrict property (@(posedge clk) req ##0 gnt);
  a0: assert property (@(posedge clk) disable iff (rst) req |=> gnt)
    else $error("miss", gnt);
  assert #0 (a);
  expect (@(posedge clk) a |-> b);
endmodule
