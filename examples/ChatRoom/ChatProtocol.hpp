#pragma once

#include <darkfire/darkfire.hpp>
#include <string>

enum class ChatMsg : uint16_t {
    RegisterHandle = 1,
    JoinRoom       = 2,
    LeaveRoom      = 3,
    ChatMessage    = 4,
    SystemNotice   = 5
};
