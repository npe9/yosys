// This test checks that we correctly elaborate interfaces in modules, even if they are loaded on
// demand. The "ondemand" module is defined in ondemand.sv in this directory and will be read as
// part of the hierarchy pass.

interface iface;
  logic [7:0] x;
  logic [7:0] y;
endinterface

module dut (input logic [7:0] x, output logic [7:0] y);
  iface intf();
  assign intf.x = x;
  assign y = intf.y;

  ondemand u(.intf);
endmodule

// fe223: was `module ref', a reserved word in SystemVerilog (IEEE 1800-2017
// 5.6.2, Annex B), now an error under -sv
module gold (input logic [7:0] x, output logic [7:0] y);
  assign y = ~x;
endmodule
