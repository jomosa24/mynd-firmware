#define LOG_MODULE_NAME "task_rpi.cpp"
#define LOG_LEVEL       LOG_LEVEL_INFO // LOG_LEVEL_DEBUG
#include "task_rpi.h"
#include "ux/rpi/rpi.h"
#include "task_bluetooth.h" // For stub implementation for RPi build
#include "task_audio.h"

#include <cstdint>
#include <cstdlib>

#include "config.h"
#include "bsp_bluetooth_uart.h"
#include "task_system.h"
#include "board_link_amps.h"
#include "board_link_io_expander.h"
#include "leds/leds.h"
#include "ux/system/system.h"
#include "ux/input/input.h"
#include "board_link_plug_detection.h"
#include "FreeRTOS.h"
#include "task.h"
#include "board.h"
#include "external/teufel/libs/GenericThread/GenericThread++.h"
#include "task_priorities.h"
#include "external/teufel/libs/core_utils/sync.h"
#include "external/teufel/libs/core_utils/overload.h"
#include "external/teufel/libs/property/property.h"

#include <cstring>
#include <cstdio>
#include "logger.h"

namespace Teufel::Task::RpiLink
{

namespace Tur = Teufel::Ux::RpiLink;

static constexpr uint32_t TASK_STACK_SIZE = 512;
static constexpr uint32_t QUEUE_SIZE      = 4;
static StaticTask_t       task_buffer;
static StackType_t        task_stack[TASK_STACK_SIZE];
static StaticQueue_t      queue_static;
static uint8_t
    queue_storage[QUEUE_SIZE * sizeof(Teufel::GenericThread::QueueMessage<Teufel::Task::RpiLink::RpiLinkMessage>)];
static Teufel::GenericThread::GenericThread<Teufel::Task::RpiLink::RpiLinkMessage> *task_handler = nullptr;
static constexpr uint8_t ot_id = static_cast<uint8_t>(Teufel::Ux::System::Task::RpiLink);

static PropertyNonOpt<Tur::Status> m_rpi_status{"rpi status", Tur::Status::RPiUnknown, Tur::Status::RPiUnknown};
PROPERTY_ENUM_SET(Tur::Status, m_rpi_status)

static PropertyNonOpt<decltype(Tur::StreamingActive::value)> m_streaming_active{"streaming active", false, false};
PROPERTY_SET(Tur::StreamingActive, m_streaming_active)

static bool send_button_events    = false;
static bool send_system_snapshots = false;
static int  last_volume_percent   = 50;
static bool override_power        = false;

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

// Helper to build and send notification messages (mcu:notify:key=value)
static void send_notify(const char *key, int value)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%s:%s:%s=%d\n", PREFIX_MCU, ACTION_NOTIFY, key, value);
    uart_write(buf);
}

// Helper to build and send notification messages with two values (mcu:notify:key=value1,value2)
static void send_notify_pair(const char *key, uint32_t value1, uint32_t value2)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s:%s:%s=%u,%u\n", PREFIX_MCU, ACTION_NOTIFY, key, value1, value2);
    uart_write(buf);
}

static void reply_kv_with_id(uint32_t id, const char *k, int v)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s:%u:%s=%d\n", PREFIX_RPI, id, k, v);
    uart_write(buf);
    log_debug("[RPILINK] Sent reply: %s", buf);
}

static void reply_error_with_id(uint32_t id, const char *msg)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "%s:%u:%s %s\n", PREFIX_RPI, id, KEY_ERROR, msg);
    uart_write(buf);
    log_debug("[RPILINK] Sent error: %s", buf);
}

static void send_button_event(uint32_t button_bitfield, uint8_t input_state)
{
    if (!send_button_events)
        return;

    // Filter out RawPress/RawRelease events - only send meaningful user actions
    if (input_state == 15 || input_state == 16) // RawPress, RawRelease
        return;

    send_notify_pair(KEY_BUTTON, button_bitfield, input_state);
    log_debug("[RPILINK] Sent button event: button=%u,%u", button_bitfield, input_state);
}

static void send_power_state(Teufel::Ux::System::PowerState state)
{
    send_notify(KEY_STATE, static_cast<int>(state));
    log_debug("[RPILINK] Sent power state: state=%d", static_cast<int>(state));
}

// Helper to parse single integer argument
static bool parse_int_arg(const char *args, int &out)
{
    if (!args || *args == '\0')
        return false;
    char *end;
    long  val = strtol(args, &end, 10);
    if (end > args)
    {
        out = static_cast<int>(val);
        return true;
    }
    return false;
}

// Helper to parse RGB arguments
static bool parse_rgb_args(const char *args, int &r, int &g, int &b)
{
    if (!args)
        return false;
    return sscanf(args, "%d %d %d", &r, &g, &b) == 3;
}

// Data integrity validation functions
static bool is_valid_prefix(const char *prefix)
{
    return prefix == PREFIX_MCU || prefix == PREFIX_RPI || prefix == PREFIX_MCU_NOTIFY;
}

static bool is_valid_action(const char *action)
{
    if (!action)
        return false;
    // Can be tokenized constant (ACTION_SET, ACTION_GET, ACTION_NOTIFY)
    // or pointer to input string (for special commands like "shutdown")
    // Special commands are validated by content, not pointer, so we allow any non-null pointer
    return true; // All non-null actions are potentially valid
}

static bool is_valid_key(const char *key)
{
    if (!key)
        return false;
    // Check if it's a tokenized constant
    return key == KEY_VOLUME || key == KEY_BFC || key == KEY_SEND_BUTTON_EVENTS || key == KEY_SEND_SYSTEM_SNAPSHOTS ||
           key == KEY_STREAMING_ACTIVE || key == KEY_LED_2 || key == KEY_OVERRIDE_POWER || key == KEY_BATTERY ||
           key == KEY_CHARGING || key == KEY_STATE;
    // Note: For notify commands, key points to input string (key=value format), so we can't validate it
}

static bool validate_tokens(const CommandTokens &tokens)
{
    if (!tokens.valid)
        return false;

    // Validate prefix
    if (!is_valid_prefix(tokens.prefix))
    {
        log_warning("[RPILINK] Invalid prefix in tokenized command");
        return false;
    }

    // Validate action (can be tokenized constant or pointer to input for special commands)
    if (!is_valid_action(tokens.action))
    {
        log_warning("[RPILINK] Invalid action in tokenized command");
        return false;
    }

    // Validate ID range (reasonable limit: 0-99999)
    if (tokens.id > 99999)
    {
        log_warning("[RPILINK] Invalid ID in tokenized command: %u", tokens.id);
        return false;
    }

    // For notify commands, key validation is skipped (points to input string)
    if (tokens.prefix == PREFIX_MCU_NOTIFY)
        return true;

    // For set/get commands, validate key if present
    if (tokens.action == ACTION_SET || tokens.action == ACTION_GET)
    {
        if (tokens.key && !is_valid_key(tokens.key))
        {
            // Key might be unknown but valid (points to input string)
            // This is acceptable for extensibility
        }
    }

    return true;
}

static CommandTokens tokenize_command(const char *line)
{
    CommandTokens tokens = {};
    tokens.valid         = false;

    if (!line || *line == '\0')
        return tokens;

    // Bounds checking: limit line length to prevent buffer overflows
    constexpr size_t MAX_LINE_LENGTH = 256;
    size_t           line_len        = strlen(line);
    if (line_len > MAX_LINE_LENGTH)
    {
        log_warning("[RPILINK] Command line too long: %zu bytes (max %zu)", line_len, MAX_LINE_LENGTH);
        return tokens;
    }

    const char *p = line;

    if (strncmp(p, PREFIX_MCU_NOTIFY, strlen(PREFIX_MCU_NOTIFY)) == 0)
    {
        tokens.prefix = PREFIX_MCU_NOTIFY;
        tokens.action = ACTION_NOTIFY;
        p += 11;
        // For notify, the rest is key=value format
        tokens.key   = p;
        tokens.args  = nullptr;
        tokens.valid = true;
        return tokens;
    }
    else if (strncmp(p, PREFIX_MCU, strlen(PREFIX_MCU)) == 0)
    {
        tokens.prefix = PREFIX_MCU;
        p += 4;
    }
    else if (strncmp(p, PREFIX_RPI, strlen(PREFIX_RPI)) == 0)
    {
        tokens.prefix = PREFIX_RPI;
        p += 4;
    }
    else
    {
        // Invalid format - no prefix found
        return tokens;
    }

    if (tokens.prefix != PREFIX_MCU_NOTIFY)
    {
        char         *end;
        unsigned long id = strtoul(p, &end, 10);
        if (end > p && *end == ':')
        {
            // Validate ID range
            if (id > 99999)
            {
                log_warning("[RPILINK] ID out of range: %lu (max 99999)", id);
                return tokens;
            }
            tokens.id = static_cast<uint32_t>(id);
            p         = end + 1;
        }
        else
        {
            // No ID, use 0
            tokens.id = 0;
        }
    }

    const char *action_start = p;
    while (*p && *p != ' ' && *p != '\0')
        p++;

    size_t action_len = p - action_start;
    if (action_len > 0)
    {
        if (strncmp(action_start, ACTION_SET, action_len) == 0)
            tokens.action = ACTION_SET;
        else if (strncmp(action_start, ACTION_GET, action_len) == 0)
            tokens.action = ACTION_GET;
        else
        {
            // Special command (like "shutdown")
            tokens.action = action_start;
            tokens.key    = nullptr;
            tokens.args   = nullptr;
            tokens.valid  = true;
            return tokens;
        }
    }

    if (*p == ' ')
        p++;

    const char *key_start = p;
    while (*p && *p != ' ' && *p != '\0')
        p++;

    size_t key_len = p - key_start;
    if (key_len > 0)
    {
        // Tokenize key by matching against known constants
        if (strncmp(key_start, KEY_VOLUME, key_len) == 0 && key_len == 6)
            tokens.key = KEY_VOLUME;
        else if (strncmp(key_start, KEY_BFC, key_len) == 0 && key_len == 3)
            tokens.key = KEY_BFC;
        else if (strncmp(key_start, KEY_SEND_BUTTON_EVENTS, key_len) == 0 && key_len == 18)
            tokens.key = KEY_SEND_BUTTON_EVENTS;
        else if (strncmp(key_start, KEY_SEND_SYSTEM_SNAPSHOTS, key_len) == 0 && key_len == 21)
            tokens.key = KEY_SEND_SYSTEM_SNAPSHOTS;
        else if (strncmp(key_start, KEY_STREAMING_ACTIVE, key_len) == 0 && key_len == 16)
            tokens.key = KEY_STREAMING_ACTIVE;
        else if (strncmp(key_start, KEY_LED_2, key_len) == 0 && key_len == 5)
            tokens.key = KEY_LED_2;
        else if (strncmp(key_start, KEY_OVERRIDE_POWER, key_len) == 0 && key_len == 14)
            tokens.key = KEY_OVERRIDE_POWER;
        else if (strncmp(key_start, KEY_BATTERY, key_len) == 0 && key_len == 7)
            tokens.key = KEY_BATTERY;
        else if (strncmp(key_start, KEY_CHARGING, key_len) == 0 && key_len == 8)
            tokens.key = KEY_CHARGING;
        else if (strncmp(key_start, KEY_STATE, key_len) == 0 && key_len == 5)
            tokens.key = KEY_STATE;
        else if (strncmp(key_start, KEY_STATUS, key_len) == 0 && key_len == 6)
            tokens.key = KEY_STATUS;
        else
            // Unknown key - keep as pointer to input string
            tokens.key = key_start;
    }

    if (*p == ' ')
        p++;

    tokens.args  = p;
    tokens.valid = true;

    // Validate tokenized data integrity
    if (!validate_tokens(tokens))
    {
        tokens.valid = false;
        return tokens;
    }

    return tokens;
}

static void handle_message(char *line)
{
    // Trim trailing newline and whitespace
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ' || line[n - 1] == '\t'))
        n--;
    line[n] = '\0'; // Null-terminate at trimmed position

    // Debug log: message received
    log_debug("[RPILINK] Received line: '%s'", line);

    CommandTokens tokens = tokenize_command(line);
    if (!tokens.valid)
    {
        log_warning("[RPILINK] Failed to parse command: '%s'", line);
        return;
    }

    // Additional integrity check: verify tokens are still valid after parsing
    if (!validate_tokens(tokens))
    {
        log_warning("[RPILINK] Token validation failed for command: '%s'", line);
        return;
    }

    if (tokens.prefix == PREFIX_MCU_NOTIFY)
    {
        log_warning("[RPILINK] Received notification from MCU (unexpected): '%s'", line);
        return;
    }

    if (tokens.prefix != PREFIX_RPI)
    {
        log_warning("[RPILINK] Command from wrong source: '%s'", line);
        return;
    }

    if (tokens.action && tokens.key == nullptr)
    {
        // For special commands, tokens.action points to the input string, so we need string comparison
        if (tokens.action == KEY_SHUTDOWN || strcmp(tokens.action, KEY_SHUTDOWN) == 0)
        {
            log_info("[RPILINK] RPi requested shutdown");
            Teufel::Task::System::postMessage(
                Teufel::Ux::System::Task::Audio,
                Teufel::Ux::System::SetPowerState{Teufel::Ux::System::PowerState::PreOff});
            reply_kv_with_id(tokens.id, KEY_SHUTDOWN, 1);
            return;
        }
        if (tokens.action == KEY_SHUTDOWN_READY || strcmp(tokens.action, KEY_SHUTDOWN_READY) == 0)
        {
            log_info("[RPILINK] RPi shutdown ready confirmation received");
            int result = Teufel::Task::RpiLink::postMessage(Teufel::Ux::System::Task::RpiLink,
                                                            Teufel::Task::RpiLink::ShutdownReady{});
            if (result != 0)
            {
                log_error("[RPILINK] Failed to post ShutdownReady message to task queue");
            }
            else
            {
                reply_kv_with_id(tokens.id, KEY_SHUTDOWN_READY, 1);
            }
            return;
        }
        reply_error_with_id(tokens.id, KEY_UNKNOWN_COMMAND);
        return;
    }

    // Dispatch based on action and key
    if (tokens.action == ACTION_SET)
    {
        if (tokens.key == KEY_VOLUME)
        {
            int v = 0;
            if (parse_int_arg(tokens.args, v))
            {
                v                   = clamp(v, 0, 100);
                last_volume_percent = v;
                board_link_amps_set_volume(percent_to_db(v));
                reply_kv_with_id(tokens.id, KEY_VOLUME, v);
            }
            else
            {
                reply_error_with_id(tokens.id, "invalid volume value");
            }
            return;
        }

        if (tokens.key == KEY_BFC)
        {
            int v = 0;
            if (parse_int_arg(tokens.args, v))
            {
                v                = clamp(v, 0, 1);
                auto charge_type = (v != 0) ? Teufel::Ux::System::ChargeType::BatteryFriendly
                                            : Teufel::Ux::System::ChargeType::FastCharge;
                int  result      = Teufel::Task::Audio::postMessage(Teufel::Ux::System::Task::RpiLink, charge_type);
                if (result != 0)
                {
                    log_error("[RPILINK] Failed to post ChargeType message to Audio task");
                    reply_error_with_id(tokens.id, "failed to set bfc");
                }
                else
                {
                    reply_kv_with_id(tokens.id, KEY_BFC, v);
                }
            }
            else
            {
                reply_error_with_id(tokens.id, "invalid bfc value");
            }
            return;
        }

        // MCU commands to RPi (these should not be handled here, but for completeness)
        if (tokens.key == KEY_SEND_BUTTON_EVENTS)
        {
            int v = 0;
            if (parse_int_arg(tokens.args, v))
            {
                send_button_events = (v != 0);
                log_debug("[RPILINK] send_button_events set to %d", send_button_events ? 1 : 0);
                reply_kv_with_id(tokens.id, KEY_SEND_BUTTON_EVENTS, send_button_events ? 1 : 0);
            }
            return;
        }

        if (tokens.key == KEY_SEND_SYSTEM_SNAPSHOTS)
        {
            int v = 0;
            if (parse_int_arg(tokens.args, v))
            {
                send_system_snapshots = (v != 0);
                log_debug("[RPILINK] send_system_snapshots set to %d", send_system_snapshots ? 1 : 0);
                reply_kv_with_id(tokens.id, KEY_SEND_SYSTEM_SNAPSHOTS, send_system_snapshots ? 1 : 0);
            }
            return;
        }

        if (tokens.key == KEY_STREAMING_ACTIVE)
        {
            int v = 0;
            if (parse_int_arg(tokens.args, v))
            {
                v = clamp(v, 0, 1);
                log_debug("[RPILINK] streaming_active set to %d", v);
                int result = Teufel::Task::RpiLink::postMessage(Teufel::Ux::System::Task::RpiLink,
                                                                Teufel::Ux::RpiLink::StreamingActive{v != 0});
                if (result != 0)
                {
                    log_error("[RPILINK] Failed to post StreamingActive message to task queue");
                    reply_error_with_id(tokens.id, "failed to set streaming_active");
                }
                else
                {
                    reply_kv_with_id(tokens.id, KEY_STREAMING_ACTIVE, v);
                }
            }
            return;
        }

        if (tokens.key == KEY_LED_2)
        {
            int r = 0, g = 0, b = 0;
            if (parse_rgb_args(tokens.args, r, g, b))
            {
                r = clamp(r, 0, 255);
                g = clamp(g, 0, 255);
                b = clamp(b, 0, 255);
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
                Teufel::Task::Leds::set_solid_color(Teufel::Task::Leds::Led::Source, color);
                char buf[64];
                snprintf(buf, sizeof(buf), "%s:%u:%s=%d,%d,%d\n", PREFIX_RPI, tokens.id, KEY_LED_2, r, g, b);
                uart_write(buf);
            }
            else
            {
                reply_error_with_id(tokens.id, "invalid led_2 format");
            }
            return;
        }

        if (tokens.key == KEY_OVERRIDE_POWER)
        {
            int v = 0;
            if (parse_int_arg(tokens.args, v))
            {
                v              = clamp(v, 0, 1);
                override_power = (v != 0);
                reply_kv_with_id(tokens.id, KEY_OVERRIDE_POWER, v);
            }
            return;
        }

        if (tokens.key == KEY_STATUS)
        {
            int v = 0;
            if (parse_int_arg(tokens.args, v))
            {
                v           = clamp(v, 0, 3); // Status enum: 0=RPiReady, 1=RPiNotReady, 2=RPiError, 3=RPiUnknown
                auto status = static_cast<Tur::Status>(v);
                int  result = Teufel::Task::RpiLink::postMessage(Teufel::Ux::System::Task::RpiLink, status);
                if (result != 0)
                {
                    log_error("[RPILINK] Failed to post Status message to task queue");
                    reply_error_with_id(tokens.id, "failed to set status");
                }
                else
                {
                    log_debug("[RPILINK] RPI status updated to %s", Tur::getDesc(status));
                    reply_kv_with_id(tokens.id, KEY_STATUS, v);
                }
            }
            else
            {
                reply_error_with_id(tokens.id, "invalid status value");
            }
            return;
        }
    }
    else if (tokens.action == ACTION_GET)
    {
        if (tokens.key == KEY_VOLUME)
        {
            reply_kv_with_id(tokens.id, KEY_VOLUME, last_volume_percent);
            return;
        }

        if (tokens.key == KEY_BFC)
        {
            auto charge_type = Teufel::Ux::System::getProperty((Teufel::Ux::System::ChargeType *) nullptr);
            int  bfc         = (charge_type == Teufel::Ux::System::ChargeType::BatteryFriendly) ? 1 : 0;
            reply_kv_with_id(tokens.id, KEY_BFC, bfc);
            return;
        }

        if (tokens.key == KEY_BATTERY)
        {
            reply_kv_with_id(tokens.id, KEY_BATTERY,
                             Teufel::Ux::System::getProperty((Teufel::Ux::System::BatteryLevel *) nullptr).value);
            return;
        }

        if (tokens.key == KEY_CHARGING)
        {
            // Map ChargerStatus to 0/1/2
            auto st = Teufel::Ux::System::getProperty((Teufel::Ux::System::ChargerStatus *) nullptr);
            int  v  = 0;
            if (st == Teufel::Ux::System::ChargerStatus::Active)
                v = 1;
            else if (Teufel::Ux::System::getProperty((Teufel::Ux::System::BatteryLevel *) nullptr).value >= 100)
                v = 2;
            reply_kv_with_id(tokens.id, KEY_CHARGING, v);
            return;
        }

        if (tokens.key == KEY_STATE)
        {
            auto ps = Teufel::Ux::System::getProperty((Teufel::Ux::System::PowerState *) nullptr);
            reply_kv_with_id(tokens.id, KEY_STATE, ps == Teufel::Ux::System::PowerState::On ? 1 : 0);
            return;
        }

        if (tokens.key == KEY_STREAMING_ACTIVE)
        {
            auto streaming = Teufel::Ux::RpiLink::getProperty((Teufel::Ux::RpiLink::StreamingActive *) nullptr);
            reply_kv_with_id(tokens.id, KEY_STREAMING_ACTIVE, streaming.value ? 1 : 0);
            return;
        }
    }

    // Unknown command
    log_warning("[RPILINK] Unrecognized command: '%s'", line);
    if (tokens.valid)
    {
        reply_error_with_id(tokens.id, KEY_UNKNOWN_COMMAND);
    }
}

static char     msg[96];
static size_t   s_len              = 0;
static uint32_t s_last_sys_emit_ms = 0;

static void idle_tick()
{
    uint8_t ch = 0;
    if (bsp_bluetooth_uart_rx(&ch, 1) == 0)
    {
        if (s_len < sizeof(msg) - 1)
        {
            msg[s_len++] = static_cast<char>(ch);
        }
        if (ch == '\n')
        {
            msg[s_len] = '\0';
            log_debug("[RPILINK] Message received: '%s'", msg);
            handle_message(msg);
            s_len = 0;
        }
    }
    else
    {
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (send_system_snapshots)
    {
        uint32_t elapsed = board_get_ms_since(s_last_sys_emit_ms);
        if (elapsed > 1000)
        {
            s_last_sys_emit_ms = get_systick();

            // Emit battery
            send_notify(KEY_BATTERY,
                        Teufel::Ux::System::getProperty((Teufel::Ux::System::BatteryLevel *) nullptr).value);
            // Emit charging
            auto st = Teufel::Ux::System::getProperty((Teufel::Ux::System::ChargerStatus *) nullptr);
            int  v  = (st == Teufel::Ux::System::ChargerStatus::Active) ? 1 : 0;
            if (Teufel::Ux::System::getProperty((Teufel::Ux::System::BatteryLevel *) nullptr).value >= 100)
                v = 2;
            send_notify(KEY_CHARGING, v);

            // Emit BFC state
            auto charge_type = Teufel::Ux::System::getProperty((Teufel::Ux::System::ChargeType *) nullptr);
            int  bfc         = (charge_type == Teufel::Ux::System::ChargeType::BatteryFriendly) ? 1 : 0;
            send_notify(KEY_BFC, bfc);

            // Do not send power state updates here, it's handled by the SetPowerState callback

            // Emit aux detect only after full power-on to ensure IO expander is initialized
            if (Teufel::Ux::System::getProperty((Teufel::Ux::System::PowerState *) nullptr) ==
                Teufel::Ux::System::PowerState::On)
            {
                send_notify(KEY_AUX, board_link_plug_detection_is_jack_connected() ? 1 : 0);
            }

            log_debug("[RPILINK] Sending periodic system updates (elapsed: %lu ms)", elapsed);
            log_debug("[RPILINK] Battery level: %d",
                      Teufel::Ux::System::getProperty((Teufel::Ux::System::BatteryLevel *) nullptr).value);
            log_debug("[RPILINK] Charger status: %s", Teufel::Ux::System::getDesc(st));
            log_debug("[RPILINK] BFC state: %s", Teufel::Ux::System::getDesc(charge_type));
            log_debug("[RPILINK] Aux detect: %s", board_link_plug_detection_is_jack_connected() ? "true" : "false");

            // These states are not sent to rpi here, but are useful for debugging
            log_debug("[RPILINK] Power state: %s", Teufel::Ux::System::getDesc(Teufel::Ux::System::getProperty(
                                                       (Teufel::Ux::System::PowerState *) nullptr)));
            auto streaming = Teufel::Ux::RpiLink::getProperty((Teufel::Ux::RpiLink::StreamingActive *) nullptr);
            log_debug("[RPILINK] Streaming active: %s", streaming.value ? "true" : "false");
            log_debug("[RPILINK] RPI status: %s", Teufel::Ux::RpiLink::getDesc(Teufel::Ux::RpiLink::getProperty(
                                                      (Teufel::Ux::RpiLink::Status *) nullptr)));
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
                            if (p.to == Teufel::Ux::System::PowerState::PreOff)
                            {
                                // Send PreOff to notify RPi daemon
                                send_power_state(p.to);
                            }
                            else if (p.to == Teufel::Ux::System::PowerState::Off)
                            {
                                // Do not shutdown RPi if charging is active
                                if (not isProperty(Teufel::Ux::System::ChargerStatus::Active))
                                {
                                    log_debug("[RPILINK] Sending Off to notify RPi daemon");
                                    send_power_state(p.to);
                                }
                            }
                            else // PreOn, On, or Transition
                            {
                                send_power_state(p.to);
                                SyncPrimitive::notify(ot_id);
                            }
                        },
                        [](const ButtonEvent &e)
                        {
                            if (isProperty(Ux::System::PowerState::On))
                            {
                                send_button_event(e.button_bitfield, e.input_state);
                            }
                        },
                        [](const ShutdownReady &)
                        {
                            log_debug("[RPILINK] Processing shutdown ready message");
                            SyncPrimitive::notify(ot_id);
                        },
                        [](const Teufel::Ux::RpiLink::Status &s)
                        {
                            log_debug("[RPILINK] status changed to %s", Teufel::Ux::RpiLink::getDesc(s));
                            setProperty(s);
                        },
                        [](const Teufel::Ux::RpiLink::StreamingActive &s)
                        {
                            log_debug("[RPILINK] streaming active: %s", s.value ? "true" : "false");
                            setProperty(s);
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

} // namespace Teufel::Task::RpiLink

namespace Teufel::Ux::RpiLink
{

// Properties public API
TS_GET_PROPERTY_NON_OPT_FN(Teufel::Task::RpiLink, m_rpi_status, Status)
TS_GET_PROPERTY_NON_OPT_FN(Teufel::Task::RpiLink, m_streaming_active, StreamingActive)
} // namespace Teufel::Ux::RpiLink

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
namespace Teufel::Ux::Bluetooth
{
Status getProperty(Ux::Bluetooth::Status *)
{
    return Status::None;
}
}
