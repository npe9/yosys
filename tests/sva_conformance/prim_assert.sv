// Clean OpenTitan assertion macros for parser conformance.
// The upstream headers continue macro lines through comments, which drops
// the line-continuation backslash during preprocessing. These definitions
// are the standard SVA shapes from prim_assert_standard_macros.svh.

`ifndef PRIM_ASSERT_SV_CONFORMANCE
`define PRIM_ASSERT_SV_CONFORMANCE

`define ASSERT_DEFAULT_CLK clk_i
`define ASSERT_DEFAULT_RST !rst_ni

`define ASSERT(__name, __prop, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  __name: assert property (@(posedge __clk) disable iff ((__rst) !== '0) (__prop)) \
    else $error("ASSERT FAILED");

`define ASSERT_NEVER(__name, __prop, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  __name: assert property (@(posedge __clk) disable iff ((__rst) !== '0) not (__prop)) \
    else $error("ASSERT FAILED");

`define ASSERT_I(__name, __prop) \
  __name: assert (__prop) else $error("ASSERT FAILED");

`define ASSUME(__name, __prop, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  __name: assume property (@(posedge __clk) disable iff ((__rst) !== '0) (__prop)) \
    else $error("ASSERT FAILED");

`define ASSUME_I(__name, __prop) \
  __name: assume (__prop) else $error("ASSERT FAILED");

`define COVER(__name, __prop, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  __name: cover property (@(posedge __clk) disable iff ((__rst) !== '0) (__prop));

`define ASSERT_KNOWN(__name, __sig, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  `ASSERT(__name, !$isunknown(__sig), __clk, __rst)

`define ASSERT_KNOWN_IF(__name, __sig, __enable, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  `ASSERT_IF(__name, !$isunknown(__sig), __enable, __clk, __rst)

`define ASSERT_IF(__name, __prop, __enable, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  `ASSERT(__name, (__enable) |-> (__prop), __clk, __rst)

`define ASSERT_PULSE(__name, __sig, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  `ASSERT(__name, $rose(__sig) |=> !(__sig), __clk, __rst)

`define ASSERT_INIT(__name, __prop) \
  initial begin : __name \
    __name``_i: assert (__prop) else $error("ASSERT FAILED"); \
  end

`define ASSERT_INIT_NET(__name, __prop) \
  initial begin : __name \
    __name``_i: assert (__prop) else $error("ASSERT FAILED"); \
  end

`define ASSERT_FINAL(__name, __prop) \
  __name: assert (__prop) else $error("ASSERT FAILED");

`define ASSERT_AT_RESET(__name, __prop, __rst = `ASSERT_DEFAULT_RST) \
  __name: assert property (@(posedge __rst) (__prop)) else $error("ASSERT FAILED");

`define ASSERT_AT_RESET_AND_FINAL(__name, __prop, __rst = `ASSERT_DEFAULT_RST) \
  `ASSERT_AT_RESET(AtReset_``__name, __prop, __rst) \
  `ASSERT_FINAL(Final_``__name, __prop)

`define ASSUME_FPV(__name, __prop, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  `ASSUME(__name, __prop, __clk, __rst)

`define COVER_FPV(__name, __prop, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  `COVER(__name, __prop, __clk, __rst)

`define ASSUME_I_FPV(__name, __prop) \
  `ASSUME_I(__name, __prop)

`define PRIM_STRINGIFY(__x) `"__x`"

// Elaboration-time assertions. The static ones are plain-Verilog tricks
// (constant function with a negative-size array when the condition is
// false), legal in packages and generate blocks alike.
`define ASSERT_STATIC_IN_PACKAGE(__name, __prop)              \
  function automatic bit assert_static_in_package_``__name(); \
    bit unused_bit [((__prop) ? 1 : -1)];                     \
    unused_bit = '{default: 1'b0};                            \
    return unused_bit[0];                                     \
  endfunction

`define ASSERT_STATIC(__name, __prop) \
  initial begin : __name \
    __name``_i: assert (__prop) else $error("ASSERT FAILED"); \
  end

`define ASSERT_STATIC_LINT_ERROR(__name, __prop)     \
  localparam int __name = (__prop) ? 1 : 2;          \
  always_comb begin                                  \
    logic unused_assert_static_lint_error;           \
    unused_assert_static_lint_error = (__name == 2); \
  end

`define ASSERT_ERROR(__name) \
  $error("ASSERT FAILED: %0s", `PRIM_STRINGIFY(__name));

`define ASSERT_FPV_LINEAR_FSM(__name, __state, __type, __clk = `ASSERT_DEFAULT_CLK, __rst = `ASSERT_DEFAULT_RST) \
  bit __name``_cond;                                                                                          \
  always_ff @(posedge __clk or posedge __rst) begin                                                           \
    if (__rst) begin                                                                                          \
      __name``_cond <= 0;                                                                                     \
    end else begin                                                                                            \
      __name``_cond <= 1;                                                                                     \
    end                                                                                                       \
  end                                                                                                         \
  property __name``_p;                                                                                        \
    __type initial_state;                                                                                     \
    (!$stable(__state) & __name``_cond, initial_state = $past(__state)) |->                                   \
        (__state != initial_state) until !(__name``_cond);                                                    \
  endproperty                                                                                                 \
  `ASSERT(__name, __name``_p, __clk, 0)

// Security-countermeasure assertion macros (prim_assert_sec_cm.svh):
// when a named error signal rises, its alert/response must follow within
// MAX_CYCLES_ cycles. These exercise hierarchical references into child
// instances (HIER_.err_o) inside property expressions.
`define _SEC_CM_ALERT_MAX_CYC 30

`define ASSERT_ERROR_TRIGGER_ERR(NAME_, HIER_, ERR_, GATE_, MAX_CYCLES_, ERR_NAME_, CLK_, RST_) \
  `ASSERT(FpvSecCm``NAME_, $rose(HIER_.ERR_NAME_) && !(GATE_) |-> ##[0:MAX_CYCLES_] (ERR_), CLK_, RST_) \
  assign HIER_.unused_assert_connected = 1'b1;

`define ASSERT_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_, MAX_CYCLES_, ERR_NAME_) \
  `ASSERT_ERROR_TRIGGER_ERR(NAME_, HIER_, (ALERT_.alert_p), GATE_, MAX_CYCLES_, ERR_NAME_, `ASSERT_DEFAULT_CLK, `ASSERT_DEFAULT_RST) \
  `ASSUME_FPV(NAME_``TriggerAfterAlertInit_S, $stable(rst_ni) == 0 |-> HIER_.ERR_NAME_ == 0 [*10])

`define ASSERT_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_, ALERT_IN_, GATE_, MAX_CYCLES_, ERR_NAME_) \
  `ASSERT_ERROR_TRIGGER_ERR(NAME_, HIER_, ALERT_IN_, GATE_, MAX_CYCLES_, ERR_NAME_, `ASSERT_DEFAULT_CLK, `ASSERT_DEFAULT_RST)

`define ASSERT_PRIM_COUNT_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_, MAX_CYCLES_, err_o)

`define ASSERT_PRIM_DOUBLE_LFSR_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_, MAX_CYCLES_, err_o)

`define ASSERT_PRIM_FSM_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_, MAX_CYCLES_, unused_err_o)

`define ASSERT_PRIM_ONEHOT_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_, MAX_CYCLES_, err_o)

`define ASSERT_PRIM_REG_WE_ONEHOT_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_PRIM_ONEHOT_ERROR_TRIGGER_ALERT(NAME_, HIER_.u_prim_reg_we_check.u_prim_onehot_check, ALERT_, GATE_, MAX_CYCLES_)

`define ASSERT_PRIM_FIFO_SYNC_SINGLETON_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_ERROR_TRIGGER_ALERT(NAME_, HIER_, ALERT_, GATE_, MAX_CYCLES_, err_o)

`define ASSERT_PRIM_COUNT_ERROR_TRIGGER_ERR(NAME_, HIER_, ERR_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC, CLK_ = `ASSERT_DEFAULT_CLK, RST_ = `ASSERT_DEFAULT_RST) \
  `ASSERT_ERROR_TRIGGER_ERR(NAME_, HIER_, ERR_, GATE_, MAX_CYCLES_, err_o, CLK_, RST_)

`define ASSERT_PRIM_DOUBLE_LFSR_ERROR_TRIGGER_ERR(NAME_, HIER_, ERR_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC, CLK_ = `ASSERT_DEFAULT_CLK, RST_ = `ASSERT_DEFAULT_RST) \
  `ASSERT_ERROR_TRIGGER_ERR(NAME_, HIER_, ERR_, GATE_, MAX_CYCLES_, err_o, CLK_, RST_)

`define ASSERT_PRIM_FSM_ERROR_TRIGGER_ERR(NAME_, HIER_, ERR_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC, CLK_ = `ASSERT_DEFAULT_CLK, RST_ = `ASSERT_DEFAULT_RST) \
  `ASSERT_ERROR_TRIGGER_ERR(NAME_, HIER_, ERR_, GATE_, MAX_CYCLES_, unused_err_o, CLK_, RST_)

`define ASSERT_PRIM_ONEHOT_ERROR_TRIGGER_ERR(NAME_, HIER_, ERR_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC, CLK_ = `ASSERT_DEFAULT_CLK, RST_ = `ASSERT_DEFAULT_RST) \
  `ASSERT_ERROR_TRIGGER_ERR(NAME_, HIER_, ERR_, GATE_, MAX_CYCLES_, err_o, CLK_, RST_)

`define ASSERT_PRIM_REG_WE_ONEHOT_ERROR_TRIGGER_ERR(NAME_, HIER_, ERR_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC, CLK_ = `ASSERT_DEFAULT_CLK, RST_ = `ASSERT_DEFAULT_RST) \
  `ASSERT_ERROR_TRIGGER_ERR(NAME_, HIER_.u_prim_reg_we_check.u_prim_onehot_check, ERR_, GATE_, MAX_CYCLES_, err_o, CLK_, RST_)

`define ASSERT_PRIM_COUNT_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_, ALERT_IN_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_, ALERT_IN_, GATE_, MAX_CYCLES_, err_o)

`define ASSERT_PRIM_DOUBLE_LFSR_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_, ALERT_IN_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_, ALERT_IN_, GATE_, MAX_CYCLES_, err_o)

`define ASSERT_PRIM_FSM_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_, ALERT_IN_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_, ALERT_IN_, GATE_, MAX_CYCLES_, unused_err_o)

`define ASSERT_PRIM_ONEHOT_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_, ALERT_IN_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_, ALERT_IN_, GATE_, MAX_CYCLES_, err_o)

`define ASSERT_PRIM_REG_WE_ONEHOT_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_, ALERT_IN_, GATE_ = 0, MAX_CYCLES_ = `_SEC_CM_ALERT_MAX_CYC) \
  `ASSERT_ERROR_TRIGGER_ALERT_IN(NAME_, HIER_.u_prim_reg_we_check.u_prim_onehot_check, ALERT_IN_, GATE_, MAX_CYCLES_, err_o)

// Register-flop macros (prim_flop_macros.sv) with assertion hooks.
`define PRIM_FLOP_CLK clk_i
`define PRIM_FLOP_RST rst_ni
`define PRIM_FLOP_RESVAL '0

`define PRIM_FLOP_A(__d, __q, __resval = `PRIM_FLOP_RESVAL, __clk = `PRIM_FLOP_CLK, __rst_n = `PRIM_FLOP_RST) \
  always_ff @(posedge __clk or negedge __rst_n) begin \
    if (!__rst_n) begin                               \
      __q <= __resval;                                \
    end else begin                                    \
      __q <= __d;                                     \
    end                                               \
  end

`define PRIM_FLOP_SPARSE_FSM(__name, __d, __q, __type, __resval = `PRIM_FLOP_RESVAL, __clk = `PRIM_FLOP_CLK, __rst_n = `PRIM_FLOP_RST, __alert_trigger_sva_en = 1) \
  prim_sparse_fsm_flop #(                           \
    .StateEnumT(__type),                            \
    .Width($bits(__type)),                          \
    .ResetValue($bits(__type)'(__resval)),          \
    .EnableAlertTriggerSVA(__alert_trigger_sva_en)  \
  ) __name (                                        \
    .clk_i   ( __clk   ),                           \
    .rst_ni  ( __rst_n ),                           \
    .state_i ( __d     ),                           \
    .state_o ( __q     )                            \
  );

`define INC_ASSERT

`endif
