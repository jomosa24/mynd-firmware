#pragma once

#include <variant>

#include "ux/system/system.h"

namespace Teufel::Task::RpiLink
{

// clang-format off
struct ShutdownReady {};
struct ButtonEvent {
    uint32_t button_bitfield;
    uint8_t input_state;
};

using RpiLinkMessage = std::variant<
    Teufel::Ux::System::SetPowerState,
    ButtonEvent,
    ShutdownReady
>;
// clang-format on

int start();

int postMessage(Teufel::Ux::System::Task source_task, RpiLinkMessage msg);

}

