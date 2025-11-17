#define LOG_MODULE_NAME "task_rpi.cpp"
#define LOG_LEVEL       LOG_LEVEL_DEBUG
#include "task_rpi.h"

#include <cstdint>
#include <cstdlib>

#include "bsp_bluetooth_uart.h"
#include "task_system.h"
#include "board_link_amps.h"
#include "leds/leds.h"
#include "ux/system/system.h"
#include "board_link_plug_detection.h"
#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "GenericThread++.h"
#include "task_priorities.h"
#include "teufel/core/utils/sync.h"
#include "teufel/core/utils/overload.h"

#include <cstring>
#include <cstdio>
#include "logger.h"

namespace Teufel::Task::RpiLink
{

static constexpr uint32_t TASK_STACK_SIZE = 512;
static constexpr uint32_t QUEUE_SIZE      = 4;
static StaticTask_t       task_buffer;
static StackType_t        task_stack[TASK_STACK_SIZE];
static StaticQueue_t      queue_static;
static uint8_t
    queue_storage[QUEUE_SIZE * sizeof(Teufel::GenericThread::QueueMessage<Teufel::Task::RpiLink::RpiLinkMessage>)];
static Teufel::GenericThread::GenericThread<Teufel::Task::RpiLink::RpiLinkMessage> *task_handler = nullptr;
static constexpr uint8_t ot_id = static_cast<uint8_t>(Teufel::Ux::System::Task::RpiLink);

static bool send_buttons        = false;
static bool send_system         = false;
static int  last_volume_percent = 50;
static bool override_power      = false;
static int  last_ecomode        = 0;
static int  last_bfc            = 0;

static int percent_to_db(int percent)
{
    // Map 0..100 to -90..10 linearly
    return -90 + ((percent * 100) / 100);
}

static int clamp(int v, int lo, int hi)
{
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static void uart_write(const char *s)
{
    bsp_bluetooth_uart_tx(reinterpret_cast<const uint8_t *>(s), strlen(s));
}

static void reply_kv(const char *k, int v)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%s=%d\n", k, v);
    uart_write(buf);
    log_debug("[RPILINK] Sent reply: %s", buf);
}

static void send_button_event(uint32_t button_bitfield, uint8_t input_state)
{
    if (!send_buttons)
        return;

    // Filter out RawPress/RawRelease events - only send meaningful user actions
    if (input_state == 15 || input_state == 16) // RawPress, RawRelease
        return;

    char buf[48];
    snprintf(buf, sizeof(buf), "button=%u,%u\n", button_bitfield, input_state);
    uart_write(buf);
    log_debug("[RPILINK] Sent button event: button=%u,%u", button_bitfield, input_state);
}

static void send_power_state(bool is_on)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "state=%d\n", is_on ? 1 : 0);
    uart_write(buf);
    log_debug("[RPILINK] Sent power state: state=%d", is_on ? 1 : 0);
}

static void handle_line(char *line)
{
    // Trim trailing newline and whitespace
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'))
        n--;
    line[n] = '\0'; // Null-terminate at trimmed position

    // Debug log: message received
    log_debug("[RPILINK] Received line: '%s' (len=%d)", line, (int) strlen(line));

    if (strncmp(line, "set override_power ", 19) == 0)
    {
        int v          = std::atoi(line + 19);
        v              = clamp(v, 0, 1);
        override_power = (v != 0);
        reply_kv("override_power", v);
        return;
    }
    if (strncmp(line, "set power_off ", 14) == 0)
    {
        int sec = std::atoi(line + 14);
        (void) sec;
        // TODO: implement timer scheduling
        reply_kv("power_off", sec);
        return;
    }
    if (strncmp(line, "set power_on ", 13) == 0)
    {
        int v = std::atoi(line + 13);
        if (v)
        {
            Teufel::Task::System::postMessage(Teufel::Ux::System::Task::Audio,
                                              Teufel::Ux::System::SetPowerState{Teufel::Ux::System::PowerState::On});
        }
        reply_kv("power_on", v);
        return;
    }
    if (strncmp(line, "set override_volume ", 21) == 0)
    {
        int v        = std::atoi(line + 21);
        v            = clamp(v, 0, 1);
        send_buttons = (v != 0); // reuse flag placeholder
        log_debug("[RPILINK] override_volume set to %d", v);
        reply_kv("override_volume", v);
        return;
    }
    // Check send_buttons command
    // After trimming, "set send_buttons 1" has no trailing space (len=18)
    // "set send_buttons " = 17 chars (space at pos 16), '1' at pos 17
    if (strlen(line) >= 17 && strncmp(line, "set send_buttons ", 17) == 0)
    {
        log_debug("[RPILINK] Matched 'set send_buttons' command");
        int v        = std::atoi(line + 17); // Skip "set send_buttons " (17 chars)
        send_buttons = (v != 0);
        log_debug("[RPILINK] send_buttons set to %d", send_buttons ? 1 : 0);
        reply_kv("send_buttons", send_buttons ? 1 : 0);
        return;
    }
    // Check send_system command
    // After trimming, "set send_system 1" has no trailing space (len=17)
    // "set send_system" = 15 chars, space at pos 15, '1' at pos 16
    if (strlen(line) >= 16 && strncmp(line, "set send_system", 15) == 0 && line[15] == ' ')
    {
        log_debug("[RPILINK] Matched 'set send_system' command");
        int v       = std::atoi(line + 16); // Skip "set send_system " (16 chars: 15 + space)
        send_system = (v != 0);
        log_debug("[RPILINK] send_system set to %d", send_system ? 1 : 0);
        reply_kv("send_system", send_system ? 1 : 0);
        return;
    }
    if (strncmp(line, "set volume ", 11) == 0)
    {
        int v               = std::atoi(line + 11);
        v                   = clamp(v, 0, 100);
        last_volume_percent = v;
        board_link_amps_set_volume(percent_to_db(v));
        reply_kv("volume", v);
        return;
    }
    if (strcmp(line, "get volume") == 0)
    {
        reply_kv("volume", last_volume_percent);
        return;
    }
    if (strncmp(line, "set ecomode ", 12) == 0)
    {
        int v = std::atoi(line + 12);
        v     = clamp(v, 0, 1);
        // TODO: post to system/audio
        reply_kv("ecomode", v);
        return;
    }
    if (strcmp(line, "get ecomode") == 0)
    {
        reply_kv("ecomode", 0);
        return;
    }
    if (strncmp(line, "set bfc ", 8) == 0)
    {
        int v = std::atoi(line + 8);
        v     = clamp(v, 0, 1);
        reply_kv("bfc", v);
        return;
    }
    if (strcmp(line, "get bfc") == 0)
    {
        reply_kv("bfc", 0);
        return;
    }
    if (strcmp(line, "get battery") == 0)
    {
        reply_kv("battery", Teufel::Ux::System::getProperty((Teufel::Ux::System::BatteryLevel *) nullptr).value);
        return;
    }
    if (strcmp(line, "get charging") == 0)
    {
        // Map ChargerStatus to 0/1/2
        auto st = Teufel::Ux::System::getProperty((Teufel::Ux::System::ChargerStatus *) nullptr);
        int  v  = 0;
        if (st == Teufel::Ux::System::ChargerStatus::Active)
            v = 1;
        else if (Teufel::Ux::System::getProperty((Teufel::Ux::System::BatteryLevel *) nullptr).value >= 100)
            v = 2;
        reply_kv("charging", v);
        return;
    }
    if (strcmp(line, "get state") == 0)
    {
        auto ps = Teufel::Ux::System::getProperty((Teufel::Ux::System::PowerState *) nullptr);
        reply_kv("state", ps == Teufel::Ux::System::PowerState::On ? 1 : 0);
        return;
    }
    if (strncmp(line, "set led_1 ", 10) == 0 || strncmp(line, "set led_2 ", 10) == 0)
    {
        // Format: set led_X r g b
        int         r = 0, g = 0, b = 0;
        const char *p = line + 10;
        while (*p == ' ')
            ++p;
        if (sscanf(p, "%d %d %d", &r, &g, &b) == 3)
        {
            r = clamp(r, 0, 255);
            g = clamp(g, 0, 255);
            b = clamp(b, 0, 255);
            if (strncmp(line, "set led_1 ", 10) == 0 && !override_power)
            {
                // Not allowed if power not overridden
                char key[7] = {};
                memcpy(key, line + 4, 5); // led_1 or led_2
                reply_kv(key, -1);        // Return error indicator
                return;
            }
            // Map to closest predefined color
            auto to_color = [](int rr, int gg, int bb)
            {
                using C = Teufel::Task::Leds::Color;
                if (rr == 0 && gg == 0 && bb == 0)
                    return C::Off;
                if (rr > 200 && gg > 200 && bb > 200)
                    return C::White;
                if (rr > gg && rr > bb)
                    return (gg > 128 ? C::Orange : C::Red);
                if (gg > rr && gg > bb)
                    return (rr > 128 ? C::Yellow : C::Green);
                if (bb > rr && bb > gg)
                    return (rr > 128 ? C::Purple : C::Blue);
                return C::White;
            };
            auto color = to_color(r, g, b);
            if (strncmp(line, "set led_1 ", 10) == 0)
                Teufel::Task::Leds::set_solid_color(Teufel::Task::Leds::Led::Status, color);
            else
                Teufel::Task::Leds::set_solid_color(Teufel::Task::Leds::Led::Source, color);
            char key[7] = {};
            memcpy(key, line + 4, 5); // led_1 or led_2
            char buf[48];
            snprintf(buf, sizeof(buf), "%s=%d,%d,%d\n", key, r, g, b);
            uart_write(buf);
        }
        else
        {
            // Invalid format - send error response
            char key[7] = {};
            memcpy(key, line + 4, 5); // led_1 or led_2
            reply_kv(key, -1);        // Return error indicator
        }
        return;
    }
    if (strcmp(line, "get power") == 0)
    {
        reply_kv("power", 0);
        return;
    }
    // Check for shutdown_ready=1 message from RPi daemon
    // "shutdown_ready=" is 15 characters
    if (strlen(line) >= 15 && strncmp(line, "shutdown_ready=", 15) == 0)
    {
        int ready = std::atoi(line + 15);
        if (ready == 1)
        {
            log_info("[RPILINK] RPi shutdown ready confirmation received");
            // Post message to task queue - this will be processed in Callback and trigger SyncPrimitive::notify
            int result = Teufel::Task::RpiLink::postMessage(Teufel::Ux::System::Task::RpiLink,
                                                            Teufel::Task::RpiLink::ShutdownReady{});
            if (result != 0)
            {
                log_error("[RPILINK] Failed to post ShutdownReady message to task queue");
            }
        }
        return;
    }

    // If we get here, no command matched
    log_warning("[RPILINK] Unrecognized command: '%s' (len=%d)", line, (int) strlen(line));
}

static char     s_line[96];
static size_t   s_len              = 0;
static uint32_t s_last_sys_emit_ms = 0;

static void idle_tick()
{
    uint8_t ch = 0;
    if (bsp_bluetooth_uart_rx(&ch, 1) == 0)
    {
        if (s_len < sizeof(s_line) - 1)
        {
            s_line[s_len++] = static_cast<char>(ch);
        }
        if (ch == '\n')
        {
            s_line[s_len] = '\0';
            log_debug("[RPILINK] Complete line received: '%s'", s_line);
            handle_line(s_line);
            s_len = 0;
        }
    }
    else
    {
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (send_system)
    {
        uint32_t elapsed = board_get_ms_since(s_last_sys_emit_ms);
        if (elapsed > 1000)
        {
            s_last_sys_emit_ms = get_systick();
            log_debug("[RPILINK] Sending periodic system updates (elapsed: %lu ms)", elapsed);
            // Emit battery
            reply_kv("battery", Teufel::Ux::System::getProperty((Teufel::Ux::System::BatteryLevel *) nullptr).value);
            // Emit charging
            auto st = Teufel::Ux::System::getProperty((Teufel::Ux::System::ChargerStatus *) nullptr);
            int  v  = (st == Teufel::Ux::System::ChargerStatus::Active) ? 1 : 0;
            if (Teufel::Ux::System::getProperty((Teufel::Ux::System::BatteryLevel *) nullptr).value >= 100)
                v = 2;
            reply_kv("charging", v);
            // Emit power state
            auto ps = Teufel::Ux::System::getProperty((Teufel::Ux::System::PowerState *) nullptr);
            reply_kv("state", ps == Teufel::Ux::System::PowerState::On ? 1 : 0);
            // Emit aux detect only after full power-on to ensure IO expander is initialized
            if (Teufel::Ux::System::getProperty((Teufel::Ux::System::PowerState *) nullptr) ==
                Teufel::Ux::System::PowerState::On)
            {
                reply_kv("aux", board_link_plug_detection_is_jack_connected() ? 1 : 0);
            }
        }
    }
}

int start()
{
    static const Teufel::GenericThread::Config<RpiLinkMessage> threadConfig = {
        .Name          = "RPi",
        .StackSize     = TASK_STACK_SIZE,
        .Priority      = TASK_SYSTEM_PRIORITY,
        .IdleMs        = 5,
        .Callback_Idle = +[]() { idle_tick(); },
        .Callback_Init =
            +[]()
            {
                bsp_bluetooth_uart_init();
                s_len              = 0;
                s_last_sys_emit_ms = get_systick();
                SyncPrimitive::notify(ot_id);
            },
        .QueueSize = QUEUE_SIZE,
        .Callback =
            +[](uint8_t /*modid*/, RpiLinkMessage msg)
            {
                std::visit(
                    Teufel::Core::overload{
                        [](const Teufel::Ux::System::SetPowerState &p)
                        {
                            bool is_on = (p.to == Teufel::Ux::System::PowerState::On);
                            send_power_state(is_on);
                            // For PreOff, don't notify yet - wait for shutdown_ready confirmation from RPi
                            // For On and Off, notify immediately
                            if (p.to != Teufel::Ux::System::PowerState::PreOff)
                            {
                                SyncPrimitive::notify(ot_id);
                            }
                        },
                        [](const ButtonEvent &e) { send_button_event(e.button_bitfield, e.input_state); },
                        [](const ShutdownReady &)
                        {
                            log_info("[RPILINK] Processing shutdown ready message");
                            SyncPrimitive::notify(ot_id);
                        },
                    },
                    msg);
            },
        .StackBuffer = task_stack,
        .StaticTask  = &task_buffer,
        .StaticQueue = &queue_static,
        .QueueBuffer = queue_storage,
    };

    task_handler = Teufel::GenericThread::create(&threadConfig);
    return task_handler ? 0 : -1;
}

int postMessage(Teufel::Ux::System::Task source_task, RpiLinkMessage msg)
{
    if (!task_handler)
    {
        return -1;
    }
    return Teufel::GenericThread::PostMsg(task_handler, static_cast<uint8_t>(source_task), msg);
}
}

