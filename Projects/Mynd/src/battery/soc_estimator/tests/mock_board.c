#include <stdint.h>

// Mock tick count to simulate passage of time
static uint32_t mock_tick_count = 0;

// Mock implementation of get_systick()
uint32_t get_systick(void)
{
    return mock_tick_count + 100;
}

// Mock implementation of board_get_ms_since()
uint32_t board_get_ms_since(uint32_t tick_ms)
{
    return (mock_tick_count >= tick_ms) ? (mock_tick_count - tick_ms) : (0xFFFFFFFF - tick_ms + mock_tick_count + 1);
}
