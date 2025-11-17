#include "board_link_bluetooth.h"
#include "logger.h"

void board_link_bluetooth_init(void)
{
    log_info("board_link_bluetooth_init() stub called");
}

void board_link_bluetooth_set_power(bool on)
{
    log_info("board_link_bluetooth_set_power(%s) stub called", on ? "on" : "off");
}

void board_link_bluetooth_reset(bool assert)
{
    log_info("board_link_bluetooth_reset(%s) stub called", assert ? "asserted" : "deasserted");
}

