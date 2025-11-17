// Stubs for Teufel::Ux::Bluetooth::getProperty to resolve linker errors
// Required for logger module name
#define LOG_MODULE_NAME "bluetooth_stub"
#include "board_link_amps.h"
#include "board.h"
#include "config.h"
#include "task_bluetooth.h"
#include "ux/system/system.h"
namespace Teufel
{
namespace Ux
{
namespace Bluetooth
{
Status getProperty(Status *)
{
    return Status::None;
}
StreamingActive getProperty(StreamingActive *)
{
    static uint32_t s_stream_inactive_timestamp = 0;
    static bool     s_last_streaming_state      = true;
    bool            is_streaming                = false;

    // Get the off timer value from system task
    auto     off_timer     = Teufel::Ux::System::getProperty(static_cast<Teufel::Ux::System::OffTimer *>(nullptr));
    uint32_t half_timer_ms = (off_timer.value * CONFIG_IDLE_POWER_OFF_TIMEOUT_MS_FACTOR) / 2;

    // Only check I2C bus if we're at least halfway through the off timer period
    // This prevents I2C bus hogging by reducing the frequency of checks
    if (s_stream_inactive_timestamp == 0 || board_get_ms_since(s_stream_inactive_timestamp) >= half_timer_ms)
    {
        // Check I2C bus only when past halfway point
        is_streaming = board_link_amps_fs_ready();

        if (is_streaming)
        {
            // Stream is active, reset the inactive timestamp
            s_stream_inactive_timestamp = 0;
            s_last_streaming_state      = true;
        }
        else
        {
            // Stream is inactive, record the timestamp if not already set
            if (s_stream_inactive_timestamp == 0)
            {
                s_stream_inactive_timestamp = get_systick();
            }
            s_last_streaming_state = false;
        }
    }
    else
    {
        // Not yet halfway through timer, return last known state without checking I2C
        is_streaming = s_last_streaming_state;
        if (!is_streaming && s_stream_inactive_timestamp == 0)
        {
            s_stream_inactive_timestamp = get_systick();
        }
    }

    return StreamingActive{is_streaming};
}
}
}
}
// Stub implementation for RPi build
namespace Teufel::Task::Bluetooth
{

int start()
{
    return 0;
}

int postMessage(Teufel::Ux::System::Task /*source_task*/, BluetoothMessage /*msg*/)
{
    return 0;
}

}

