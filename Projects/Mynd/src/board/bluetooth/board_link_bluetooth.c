#include "board_link_bluetooth.h"
#include "board_hw.h"
#define LOG_LEVEL LOG_LEVEL_ERROR
#include "logger.h"

static bool is_bluetooth_gpios_initialized = false;

void board_link_bluetooth_init(void)
{
#ifndef MYND_RPI_MODIFICATION
    GPIO_InitTypeDef GPIO_InitStruct;

    // Configure BT Vreg control pin
    BT_CTRL_VREG_GPIO_CLK_ENABLE();
    GPIO_InitStruct.Pin   = BT_CTRL_VREG_GPIO_PIN;
    GPIO_InitStruct.Mode  = BT_CTRL_VREG_GPIO_MODE;
    GPIO_InitStruct.Pull  = BT_CTRL_VREG_GPIO_PULL;
    GPIO_InitStruct.Speed = BT_CTRL_VREG_GPIO_SPEED;
    HAL_GPIO_Init(BT_CTRL_VREG_GPIO_PORT, &GPIO_InitStruct);

    // Configure BT control 3V3 pin
    BT_CTRL_3V3_GPIO_CLK_ENABLE();
    GPIO_InitStruct.Pin   = BT_CTRL_3V3_GPIO_PIN;
    GPIO_InitStruct.Mode  = BT_CTRL_3V3_GPIO_MODE;
    GPIO_InitStruct.Pull  = BT_CTRL_3V3_GPIO_PULL;
    GPIO_InitStruct.Speed = BT_CTRL_3V3_GPIO_SPEED;
    HAL_GPIO_Init(BT_CTRL_3V3_GPIO_PORT, &GPIO_InitStruct);

    // Configure BT reset pin
    BT_RESET_GPIO_CLK_ENABLE();
    GPIO_InitStruct.Pin   = BT_RESET_GPIO_PIN;
    GPIO_InitStruct.Mode  = BT_RESET_GPIO_MODE;
    GPIO_InitStruct.Pull  = BT_RESET_GPIO_PULL;
    GPIO_InitStruct.Speed = BT_RESET_GPIO_SPEED;
    HAL_GPIO_Init(BT_RESET_GPIO_PORT, &GPIO_InitStruct);

    is_bluetooth_gpios_initialized = true;
#endif // MYND_RPI_MODIFICATION
}

void board_link_bluetooth_set_power(bool on)
{
#ifndef MYND_RPI_MODIFICATION
    HAL_GPIO_WritePin(BT_CTRL_VREG_GPIO_PORT, BT_CTRL_VREG_GPIO_PIN, (on) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BT_CTRL_3V3_GPIO_PORT, BT_CTRL_3V3_GPIO_PIN, (on) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    log_info("Bluetooth powered %s", on ? "on" : "off");
#endif // MYND_RPI_MODIFICATION
}

void board_link_bluetooth_reset(bool assert)
{
#ifndef MYND_RPI_MODIFICATION
    HAL_GPIO_WritePin(BT_RESET_GPIO_PORT, BT_RESET_GPIO_PIN, (assert) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    log_info("Bluetooth reset %s", assert ? "asserted" : "deasserted");
#endif // MYND_RPI_MODIFICATION
}
