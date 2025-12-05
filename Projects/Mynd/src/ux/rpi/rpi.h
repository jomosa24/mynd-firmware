#pragma once

#include <cstdint>

namespace Teufel::Ux::RpiLink
{
// clang-format off
struct ShutdownReady {};
struct ButtonEvent {
    uint32_t button_bitfield;
    uint8_t input_state;
};

// clang-format on
enum class Status : uint8_t
{
    RPiReady,
    RPiNotReady,
    RPiError,
    RPiUnknown,
};

inline auto getDesc(const Status &value)
{
    switch (value)
    {
        case Status::RPiReady:
            return "RPiReady";
        case Status::RPiNotReady:
            return "RPiNotReady";
        case Status::RPiError:
            return "RPiError";
        case Status::RPiUnknown:
            return "RPiUnknown";
        default:
            return "Unknown";
    }
}

struct StreamingActive
{
    bool value;
};
// clang-format on

// Public API
Status          getProperty(Status *);
StreamingActive getProperty(StreamingActive *);
}
