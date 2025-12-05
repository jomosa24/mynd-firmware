#pragma once

#include <variant>

#include "ux/rpi/rpi.h"
#include "ux/system/system.h"

namespace Teufel::Task::RpiLink
{

// Tokenized prefix constants (for pointer comparison)
static const char *const PREFIX_MCU        = "mcu";
static const char *const PREFIX_RPI        = "rpi";
static const char *const PREFIX_MCU_NOTIFY = "mcu:notify";

// Tokenized action constants (for pointer comparison)
static const char *const ACTION_SET    = "set";
static const char *const ACTION_GET    = "get";
static const char *const ACTION_NOTIFY = "notify";

// Tokenized key constants (for pointer comparison)
static const char *const KEY_VOLUME                = "volume";
static const char *const KEY_BFC                   = "bfc";
static const char *const KEY_BUTTON                = "button";
static const char *const KEY_SEND_BUTTON_EVENTS    = "send_button_events";
static const char *const KEY_SEND_SYSTEM_SNAPSHOTS = "send_system_snapshots";
static const char *const KEY_STREAMING_ACTIVE      = "streaming_active";
static const char *const KEY_AUX                   = "aux";
static const char *const KEY_LED_2                 = "led_2";
static const char *const KEY_OVERRIDE_POWER        = "override_power";
static const char *const KEY_BATTERY               = "battery";
static const char *const KEY_CHARGING              = "charging";
static const char *const KEY_STATE                 = "state";
static const char *const KEY_STATUS                = "status";
static const char *const KEY_SHUTDOWN              = "shutdown";
static const char *const KEY_SHUTDOWN_READY        = "shutdown_ready";
static const char *const KEY_UNKNOWN_COMMAND       = "unknown_command";
static const char *const KEY_INVALID_VALUE         = "invalid_value";
static const char *const KEY_ERROR                 = "error";

// Tokenizer structure for parsing commands
struct CommandTokens
{
    const char *prefix; // Points to PREFIX_MCU, PREFIX_RPI, or PREFIX_MCU_NOTIFY
    uint32_t    id;     // request ID
    const char *action; // "set", "get", "notify", or special command
    const char *key;    // "volume", "bfc", "button", etc.
    const char *args;   // Remaining arguments
    bool        valid;  // Whether parsing succeeded
};

// clang-format off
struct ShutdownReady {};
struct ButtonEvent {
    uint32_t button_bitfield;
    uint8_t input_state;
};

using RpiLinkMessage = std::variant<
    Teufel::Ux::System::SetPowerState,
    ButtonEvent,
    ShutdownReady,
    Teufel::Ux::RpiLink::Status,
    Teufel::Ux::RpiLink::StreamingActive
>;
// clang-format on

int start();

int postMessage(Teufel::Ux::System::Task source_task, RpiLinkMessage msg);

}
