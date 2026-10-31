module tmem(input clk, input [2:0] wa, wb, ra, input [7:0] da, db, input [1:0] bea,
            input wea, web, input [3:0] romaddr);
  reg [7:0] mem [0:4];   // 5 words on a 3-bit address: 5..7 are out of range
  always @(posedge clk) begin
    if (wea) begin
      if (bea[0]) mem[wa][3:0] <= da[3:0];
      if (bea[1]) mem[wa][7:4] <= da[7:4];
    end
    if (web) mem[wb] <= db;  // the later write wins
  end
  wire [7:0] qa = mem[ra];
  reg [7:0] rom;
  always @* case (romaddr)
    4'd0: rom = 8'h12; 4'd1: rom = 8'h34; 4'd2: rom = 8'h56; 4'd3: rom = 8'h78;
    4'd4: rom = 8'h9a; 4'd5: rom = 8'hbc; 4'd6: rom = 8'hde; 4'd7: rom = 8'hf0;
    4'd8: rom = 8'h0f; 4'd9: rom = 8'h1e; 4'd10: rom = 8'h2d; 4'd11: rom = 8'h3c;
    4'd12: rom = 8'h4b; 4'd13: rom = 8'h5a; 4'd14: rom = 8'h69; default: rom = 8'h87;
  endcase
  reg both = 0; reg [2:0] pa = 0; reg [7:0] pda = 0, pdb = 0;
  always @(posedge clk) begin
    both <= wea && web && wa == wb && bea == 2'b11 && wa < 3'd5;
    pa <= wa; pda <= da; pdb <= db;
  end
  p_zero: assert property (@(posedge clk) qa == 8'd0);                    // disproved, depth 1
  p_zero_in: assert property (@(posedge clk) ra >= 3'd5 || qa == 8'd0);  // in range: disproved, depth 1
  p_nib_in: assert property (@(posedge clk) ra >= 3'd5 || qa != 8'h05);   // a half write: disproved, depth 1
  p_oor:  assert property (@(posedge clk) ra < 3'd5 || qa == 8'd0);       // holds
  p_prio: assert property (@(posedge clk) !(both && ra == pa) || qa == pdb);  // holds
  p_prio_neg: assert property (@(posedge clk) !(both && ra == pa) || qa == pda);  // disproved, depth 1
  p_nib:  assert property (@(posedge clk) !(qa[3:0] == 4'h5 && qa[7:4] == 4'h0) || !wea || web); // disproved, depth 1
  p_rom0: assert property (@(posedge clk) romaddr != 4'd0 || rom == 8'h12); // holds
  p_rom1: assert property (@(posedge clk) rom != 8'h34);                   // disproved, depth 0
endmodule
module tmem2(input clk, input [2:0] wa, ra, rb, input [7:0] da, input wea);
  reg [7:0] mem [0:7];
  always @(posedge clk) if (wea) mem[wa] <= da;
  wire [7:0] qa = mem[ra], qb = mem[rb];
  p_two: assert property (@(posedge clk) ra == rb || qa != 8'ha5 || qb != 8'h5a);  // disproved, depth 2
endmodule
// a 2-D unpacked array of continuous assigns whose rows are passed to an
// array port from a generate loop (aes_ghash's mux inputs): asynchronous
// write ports with constant addresses, memory_map's "static" words
module tsel3(input [7:0] in_i [3], input [1:0] i, output [7:0] o);
  assign o = in_i[i];
endmodule
module tmem3(input clk, input [7:0] x, y, input [1:0] ra);
  logic [7:0] arr [2][3];
  assign arr[0][0] = x; assign arr[0][1] = y; assign arr[0][2] = x ^ y;
  assign arr[1][0] = y; assign arr[1][1] = 8'h5a; assign arr[1][2] = ~x;
  wire [7:0] q [2];
  for (genvar g = 0; g < 2; g++) begin : gen_sel
    tsel3 u_sel (.in_i(arr[g]), .i(ra), .o(q[g]));
  end
  p_st:     assert property (@(posedge clk) ra != 2'd2 || q[0] == (x ^ y));  // holds
  p_st3:    assert property (@(posedge clk) ra != 2'd1 || q[1] == 8'h5a);    // holds
  p_st_neg: assert property (@(posedge clk) ra != 2'd1 || q[0] == x);        // disproved, depth 0
endmodule
