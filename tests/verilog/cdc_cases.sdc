# SDC constraints for the cdc gate (read_sdc subset parser).
#
# cdc_constrained's clocks are declared asynchronous -- and cclk_a
# carries a second, faster clock with a false path into cclk_b, so the
# pairing must check EVERY (src clock, dst clock) combination before
# downgrading a crossing.

create_clock -period 10.0 -name cclk_a [get_ports cclk_a]
create_clock -period 10.0 -name cclk_b [get_ports cclk_b]
create_clock -period 4.0  -name cclk_a_fast [get_ports cclk_a]

set_clock_groups -asynchronous \
    -group [get_clocks cclk_a] \
    -group [get_clocks cclk_b]

set_false_path -from [get_clocks cclk_a_fast] -to [get_clocks cclk_b]

set_input_delay 2.0 -clock [get_clocks cclk_a] [get_ports din_c]
set_output_delay 1.5 -clock [get_clocks cclk_b] [get_ports dout_c]

set_multicycle_path 3 -from [get_clocks cclk_a] -to [get_clocks cclk_b] -setup
set_max_delay 12.0 -from [get_clocks cclk_a] -to [get_clocks cclk_b]

create_generated_clock -name cclk_div -divide_by 2 \
    -source [get_ports cclk_a] [get_pins div_ff_reg/Q]
